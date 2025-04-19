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

// Main function for sending final measurements to Firebase
esp_err_t send_final_measurements_to_firebase(const char *firestore_data) {

    
    size_t data_size = strlen(firestore_data);
    ESP_LOGI(TAG, "Retrieved Firestore data, size: %zu bytes", data_size);
    ESP_LOGI(TAG, "Current heap: %" PRIu32 " bytes free", esp_get_free_heap_size());
    
    // Проверим, что данные корректные
    cJSON *firestore_doc = cJSON_Parse(firestore_data);
    if (!firestore_doc) {
        ESP_LOGE(TAG, "Invalid Firestore format: parse error");
        free(firestore_data);
        return ESP_FAIL;
    }
    
    // Извлечем MAC для использования в ID документа
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

    // ВАЖНО: Установите 1 для автоматической генерации ID от Firestore
    // Установите 0 для использования ID на основе текущей даты
    #define USE_AUTO_ID 0
    
    esp_err_t result;
    
    if (USE_AUTO_ID) {
        ESP_LOGI(TAG, "Using auto-generated document ID from Firestore");
        result = firebase_send_firestore_data("daily_measurements", NULL, firestore_data);
    } else {
        // Генерируем ID документа на основе текущей даты
        static char doc_id[32]; // увеличиваем буфер для безопасности
        
        // Получаем текущее время
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // Форматируем в виде YYYY-MM-DD
        strftime(doc_id, sizeof(doc_id), "%Y-%m-%d", &timeinfo);
        
        ESP_LOGI(TAG, "Using custom document ID: %s", doc_id);
        result = firebase_send_firestore_data("daily_measurements", doc_id, firestore_data);
    }
    
    // Освобождаем ресурсы
    cJSON_Delete(firestore_doc);
    free(firestore_data);
    
    // Проверяем результат отправки
    if (result == ESP_OK) {
        // Удаляем только файл Firestore-формата, очистку измерений будет выполнять main.c
        unlink(FIRESTORE_MEASUREMENTS_FILE);
        ESP_LOGI(TAG, "Measurements sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send measurements");
    }
    
    return result;
}
