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
#include "storage.h"
#include <unistd.h>

static const char *TAG = "FIREBASE_TASKS";

// Increased JSON buffer size to handle large data
#define FIRESTORE_MEASUREMENTS_FILE "/spiffs/firestore_measurements.json"

// IMPORTANT: Set 1 for automatic generation of ID from Firestore
// Set 0 for using ID based on current date
#define USE_AUTO_ID 0

// Main function for sending final measurements to Firebase
esp_err_t send_final_measurements_to_firebase(const char *firestore_data) {

    
    size_t data_size = strlen(firestore_data);
    ESP_LOGI(TAG, "Retrieved Firestore data, size: %zu bytes", data_size);
    ESP_LOGI(TAG, "Current heap: %" PRIu32 " bytes free", esp_get_free_heap_size());
    
    // Check if data is correct
    cJSON *firestore_doc = cJSON_Parse(firestore_data);
    if (!firestore_doc) {
        ESP_LOGE(TAG, "Invalid Firestore format: parse error");
        free(firestore_data);
        return ESP_FAIL;
    }
    
    // Extract MAC for use in document ID
    cJSON *fields = cJSON_GetObjectItem(firestore_doc, "fields");
    if (!fields) {
        ESP_LOGE(TAG, "Invalid Firestore format: missing fields");
        cJSON_Delete(firestore_doc);
        free(firestore_data);
        return ESP_FAIL;
    }
    
    cJSON *tag_id_field = cJSON_GetObjectItem(fields, "tag_id");
    if (!tag_id_field) {
        ESP_LOGE(TAG, "Invalid Firestore format: missing tag_id");
        cJSON_Delete(firestore_doc);
        free(firestore_data);
        return ESP_FAIL;
    }
    
    cJSON *tag_id_value = cJSON_GetObjectItem(tag_id_field, "stringValue");
    if (!tag_id_value || !tag_id_value->valuestring) {
        ESP_LOGE(TAG, "Invalid Firestore format: invalid tag_id");
        cJSON_Delete(firestore_doc);
        free(firestore_data);
        return ESP_FAIL;
    }
    
    cJSON *day_field = cJSON_GetObjectItem(fields, "day");
    if (!day_field) {
        ESP_LOGE(TAG, "Invalid Firestore format: missing day");
        cJSON_Delete(firestore_doc);
        free(firestore_data);
        return ESP_FAIL;
    }
    
    cJSON *day_value = cJSON_GetObjectItem(day_field, "stringValue");
    if (!day_value || !day_value->valuestring) {
        ESP_LOGE(TAG, "Invalid Firestore format: invalid day");
        cJSON_Delete(firestore_doc);
        free(firestore_data);
        return ESP_FAIL;
    }


    
    esp_err_t result;
    
    if (USE_AUTO_ID) {
        ESP_LOGI(TAG, "Using auto-generated document ID from Firestore");
        result = firebase_send_streamed_data("daily_measurements", NULL, firestore_data);
    } else {
        // Generate document ID based on current date and MAC address
        static char doc_id[64]; 
        
        // Get current time
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // Extract MAC address from the Firestore data
        char mac_address[18] = "unknown";
        cJSON *json = cJSON_Parse(firestore_data);
        if (json) {
            cJSON *fields = cJSON_GetObjectItem(json, "fields");
            if (fields) {
                cJSON *tag_id = cJSON_GetObjectItem(fields, "tag_id");
                if (tag_id) {
                    cJSON *string_value = cJSON_GetObjectItem(tag_id, "stringValue");
                    if (string_value && cJSON_IsString(string_value)) {
                        strncpy(mac_address, string_value->valuestring, sizeof(mac_address) - 1);
                        mac_address[sizeof(mac_address) - 1] = '\0';
                    }
                }
            }
            cJSON_Delete(json);
        }
        
        // Format in YYYY-MM-DD_MAC format
        strftime(doc_id, sizeof(doc_id), "%Y-%m-%d_", &timeinfo);
        strncat(doc_id, mac_address, sizeof(doc_id) - strlen(doc_id) - 1);
        
        ESP_LOGI(TAG, "Using custom document ID: %s", doc_id);
        result = firebase_send_streamed_data("daily_measurements", doc_id, firestore_data);
    }
    
    // Release resources
    cJSON_Delete(firestore_doc);
    free(firestore_data);
    
    // Check result of sending
    if (result == ESP_OK) {
        // Remove only Firestore-format file, cleaning measurements will be done by main.c
        unlink(FIRESTORE_MEASUREMENTS_FILE);
        ESP_LOGI(TAG, "Measurements sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send measurements");
    }
    
    return result;
}
