/**
 * @file firebase_tasks.c
 * @brief Implementation of Firebase data sending through separate tasks
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "firebase_api.h"
#include "time_manager.h"

static const char *TAG = "FIREBASE_TASKS";

// Firebase data sending task structure
typedef struct {
    char collection[64];         // Firestore collection name
    char document_id[64];        // Document ID
    char json_data[8192];        // JSON data to send
    SemaphoreHandle_t done_semaphore;
    esp_err_t result;
} firebase_task_data_t;

// Task for sending data to Firebase
static void firebase_send_task(void *pvParameters) {
    firebase_task_data_t *task_data = (firebase_task_data_t *)pvParameters;
    
    // Document ID could be NULL or empty for auto-generated ID
    const char *doc_id = NULL;
    if (task_data->document_id[0] != '\0') {
        doc_id = task_data->document_id;
    }
    
    // Send data
    task_data->result = firebase_send_data(task_data->collection, doc_id, task_data->json_data);
    
    // Signal completion
    xSemaphoreGive(task_data->done_semaphore);
    vTaskDelete(NULL);
}

// Function for safely sending data to Firebase via a separate task
static esp_err_t firebase_send_data_safe(const char *collection, const char *document_id, const char *json_data) {
    if (!collection || !json_data) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check JSON data size
    size_t json_len = strlen(json_data);
    if (json_len >= 8192) {
        ESP_LOGE(TAG, "JSON data too large (%zu bytes), max 8192 bytes", json_len);
        return ESP_ERR_INVALID_SIZE;
    }

    // Allocate memory for task data structure
    firebase_task_data_t *task_data = malloc(sizeof(firebase_task_data_t));
    if (!task_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for task data");
        return ESP_ERR_NO_MEM;
    }

    // Create semaphore for synchronization
    task_data->done_semaphore = xSemaphoreCreateBinary();
    if (!task_data->done_semaphore) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        free(task_data);
        return ESP_ERR_NO_MEM;
    }

    // Copy collection name to task data
    strncpy(task_data->collection, collection, sizeof(task_data->collection) - 1);
    task_data->collection[sizeof(task_data->collection) - 1] = '\0';

    // Copy document_id to task data (if provided)
    if (document_id) {
        strncpy(task_data->document_id, document_id, sizeof(task_data->document_id) - 1);
        task_data->document_id[sizeof(task_data->document_id) - 1] = '\0';
    } else {
        task_data->document_id[0] = '\0';
    }

    // Copy JSON data to task data
    strncpy(task_data->json_data, json_data, sizeof(task_data->json_data) - 1);
    task_data->json_data[sizeof(task_data->json_data) - 1] = '\0';

    // Create task with larger stack
    BaseType_t task_created = xTaskCreate(
        firebase_send_task,
        "firebase_send_task",
        12288,  // Stack size
        task_data,
        5,      // Priority
        NULL
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Firebase send task");
        vSemaphoreDelete(task_data->done_semaphore);
        free(task_data);
        return ESP_ERR_NO_MEM;
    }

    // Wait for task completion with timeout
    esp_err_t result;
    if (xSemaphoreTake(task_data->done_semaphore, pdMS_TO_TICKS(30000)) != pdTRUE) {
        ESP_LOGE(TAG, "Firebase send task timeout");
        vSemaphoreDelete(task_data->done_semaphore);
        return ESP_ERR_TIMEOUT;
    }

    // Save the result
    result = task_data->result;

    // Clean up
    vSemaphoreDelete(task_data->done_semaphore);
    free(task_data);

    return result;
}

// Main function for sending final measurements to Firebase
esp_err_t send_final_measurements_to_firebase(const char *measurements) {
    if (!measurements) {
        ESP_LOGE(TAG, "Measurements data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Check JSON validity and get the number of elements
    cJSON *measurements_array = cJSON_Parse(measurements);
    if (!measurements_array) {
        ESP_LOGE(TAG, "JSON parse error: %s", cJSON_GetErrorPtr());
        return ESP_FAIL;
    }
    
    int total_items = cJSON_GetArraySize(measurements_array);
    ESP_LOGI(TAG, "Total measurements to send: %d", total_items);
    
    if (total_items == 0) {
        ESP_LOGW(TAG, "No measurements to send");
        cJSON_Delete(measurements_array);
        return ESP_OK;
    }

    // Get the first array element and its MAC address for tag_id
    cJSON *first_item = cJSON_GetArrayItem(measurements_array, 0);
    cJSON *mac_json = cJSON_GetObjectItem(first_item, "mac");
    
    if (!mac_json || !mac_json->valuestring) {
        ESP_LOGE(TAG, "Invalid MAC address in first measurement");
        cJSON_Delete(measurements_array);
        return ESP_FAIL;
    }
    
    // Get date from the first timestamp
    cJSON *time_json = cJSON_GetObjectItem(first_item, "time");
    if (!time_json || !time_json->valuestring) {
        ESP_LOGE(TAG, "Invalid timestamp in first measurement");
        cJSON_Delete(measurements_array);
        return ESP_FAIL;
    }
    
    // Extract date from timestamp (YYYY-MM-DD HH:MM:SS)
    char day[11] = {0}; // YYYY-MM-DD\0
    strncpy(day, time_json->valuestring, 10);
    day[10] = '\0';
    
    // Create the main JSON object for sending
    cJSON *data_object = cJSON_CreateObject();
    cJSON_AddStringToObject(data_object, "tag_id", mac_json->valuestring);
    cJSON_AddStringToObject(data_object, "day", day);
    
    // Create measurements array
    cJSON *measurements_list = cJSON_CreateArray();
    
    // Add all measurements to the array
    for (int i = 0; i < total_items; i++) {
        cJSON *measurement = cJSON_GetArrayItem(measurements_array, i);
        if (!measurement) {
            continue;
        }
        
        // Extract required fields
        cJSON *temp_json = cJSON_GetObjectItem(measurement, "t");
        cJSON *hum_json = cJSON_GetObjectItem(measurement, "h");
        cJSON *item_time_json = cJSON_GetObjectItem(measurement, "time");
        
        if (!temp_json || !temp_json->valuestring || 
            !hum_json || !hum_json->valuestring || 
            !item_time_json || !item_time_json->valuestring) {
            ESP_LOGW(TAG, "Skipping measurement with missing fields at index %d", i);
            continue;
        }
        
        // Extract only time from timestamp (HH:MM:SS)
        char timestamp[9] = {0};
        const char *full_time = item_time_json->valuestring;
        if (strlen(full_time) >= 19) {
            strncpy(timestamp, full_time + 11, 8);
            timestamp[8] = '\0';
        } else {
            strncpy(timestamp, full_time, 8);
            timestamp[8] = '\0';
        }
        
        // Create an object for the current measurement
        cJSON *measurement_object = cJSON_CreateObject();
        cJSON_AddNumberToObject(measurement_object, "temperature", atof(temp_json->valuestring));
        cJSON_AddNumberToObject(measurement_object, "humidity", atof(hum_json->valuestring));
        cJSON_AddStringToObject(measurement_object, "timestamp", timestamp);
        
        // Add measurement object to array
        cJSON_AddItemToArray(measurements_list, measurement_object);
    }
    
    // Add measurements array to the main object
    cJSON_AddItemToObject(data_object, "measurements", measurements_list);
    
    // Convert object to JSON string
    char *json_data = cJSON_PrintUnformatted(data_object);
    
    // Send data to Firebase
    esp_err_t result = firebase_send_data_safe("daily_measurements", NULL, json_data);
    
    // Free resources
    free(json_data);
    cJSON_Delete(data_object);
    cJSON_Delete(measurements_array);
    
    return result;
}
