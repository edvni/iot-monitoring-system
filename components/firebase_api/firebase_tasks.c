/**
 * @file firebase_tasks.c
 * @brief Implementation of Firebase data sending through separate tasks
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>  // For unlink function
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "firebase_api.h"

static const char *TAG = "FIREBASE_TASKS";

// Firebase data sending task structure
typedef struct {
    char collection[64]; // Firestore collection name
    char document_id[64]; // Document ID (use NULL for auto-generated ID)
    char json_data[4096]; // JSON data to send
    SemaphoreHandle_t done_semaphore;
    esp_err_t result;
} firebase_task_data_t;

time_t parse_timestamp(const char *timestamp_str) {
    struct tm tm = {0};
    strptime(timestamp_str, "%Y-%m-%d %H:%M:%S", &tm);
    return mktime(&tm);
}

// Task for sending Firebase data with a larger stack
static void firebase_send_task(void *pvParameters) {
    firebase_task_data_t *task_data = (firebase_task_data_t *)pvParameters;
    
    ESP_LOGI(TAG, "Firebase send task started, JSON data length: %zu", strlen(task_data->json_data));
    
    // Document ID could be NULL or empty for auto-generated ID
    const char *doc_id = NULL;
    if (task_data->document_id[0] != '\0') {
        doc_id = task_data->document_id;
    }
    
    // Send the data
    task_data->result = firebase_send_data(task_data->collection, doc_id, task_data->json_data);
    
    // Signal completion
    xSemaphoreGive(task_data->done_semaphore);
    vTaskDelete(NULL);
}

// Function to send Firebase data using a separate task
esp_err_t firebase_send_data_safe(const char *collection, const char *document_id, const char *json_data) {
    if (collection == NULL || json_data == NULL) {
        ESP_LOGE(TAG, "Invalid parameters: Collection or json_data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate memory for task data structure
    firebase_task_data_t *task_data = malloc(sizeof(firebase_task_data_t));
    if (task_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for task data");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Creating Firebase send task");

    // Create semaphore for synchronization
    task_data->done_semaphore = xSemaphoreCreateBinary();
    if (task_data->done_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        free(task_data);
        return ESP_ERR_NO_MEM;
    }

    // Copy collection name to task data
    size_t collection_len = strlen(collection);
    size_t max_collection_copy = sizeof(task_data->collection) - 1;

    if (collection_len > max_collection_copy) {
        ESP_LOGW(TAG, "Collection name length (%zu) exceeds buffer size (%zu), will be truncated",
                 collection_len, max_collection_copy);
    }

    strncpy(task_data->collection, collection, max_collection_copy);
    task_data->collection[max_collection_copy] = '\0'; // Ensure null terminator

    // Copy document_id to task data (if provided)
    if (document_id != NULL) {
        size_t doc_id_len = strlen(document_id);
        size_t max_doc_id_copy = sizeof(task_data->document_id) - 1;

        if (doc_id_len > max_doc_id_copy) {
            ESP_LOGW(TAG, "Document ID length (%zu) exceeds buffer size (%zu), will be truncated",
                     doc_id_len, max_doc_id_copy);
        }
        strncpy(task_data->document_id, document_id, max_doc_id_copy);
        task_data->document_id[max_doc_id_copy] = '\0'; // Ensure null terminator
    } else {
        task_data->document_id[0] = '\0'; // Set to empty string if NULL
    }

    // Copy JSON data to task data
    size_t json_len = strlen(json_data);
    size_t max_json_copy = sizeof(task_data->json_data) - 1;

    if (json_len > max_json_copy) {
        ESP_LOGW(TAG, "JSON data length (%zu) exceeds buffer size (%zu), will be truncated",
                 json_len, max_json_copy);
    }
    strncpy(task_data->json_data, json_data, max_json_copy);
    task_data->json_data[max_json_copy] = '\0'; // Ensure null terminator

    // Create task with larger stack
    BaseType_t task_created = xTaskCreate(
        firebase_send_task,
        "firebase_send_task",
        12288,  // Stack size
        task_data,  // Pass pointer to allocated memory
        5,     // Priority
        NULL
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Firebase send task");
        vSemaphoreDelete(task_data->done_semaphore);
        free(task_data);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Waiting for Firebase send task to complete");

    // Wait for task completion with timeout
    esp_err_t result;
    if (xSemaphoreTake(task_data->done_semaphore, pdMS_TO_TICKS(30000)) != pdTRUE) {
        ESP_LOGE(TAG, "Firebase send task timeout");
        // Don't free task_data as the task might still be using it
        vSemaphoreDelete(task_data->done_semaphore);
        return ESP_ERR_TIMEOUT;
    }

    // Save the result
    result = task_data->result;

    ESP_LOGI(TAG, "Firebase send task completed with result: %s",
             result == ESP_OK ? "OK" : "Failed");

    // Clean up
    vSemaphoreDelete(task_data->done_semaphore);
    free(task_data);

    return result;
}

// Function to send sensor data with retries using a separate task
esp_err_t firebase_send_sensor_data_with_retries(const char *tag_id, float temperature, float humidity, const char *timestamp_str, int max_retries) {
    esp_err_t ret = ESP_FAIL;

    time_t timestamp = parse_timestamp(timestamp_str); // Convert time string to Unix time

    // Create JSON data for the sensor reading
    char json_data[256];
    snprintf(json_data, sizeof(json_data), 
        "{\"tag_id\": \"%s\", \"temperature\": %.2f, \"humidity\": %.2f, \"timestamp\": %ld}",
        tag_id, temperature, humidity, (long)timestamp);

    for (int i = 0; i < max_retries; i++) {
        // Send data to "measurements" collection with auto-generated document ID
        ret = firebase_send_data_safe("measurements", NULL, json_data);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Succesfully sent sensor data to Cloud Firestore");
            return ESP_OK;
        }

        ESP_LOGI(TAG, "Failed to send sensor data, retrying (%d/%d)", i + 1, max_retries);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait before retrying
    }

    ESP_LOGE(TAG, "All sensor data send retries failed");
    return ret;
}
