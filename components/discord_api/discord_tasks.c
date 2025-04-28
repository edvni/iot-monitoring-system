/**
 * @file discord_tasks.c
 * @brief Implementation of Discord message sending through separate tasks
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "discord_api.h"

static const char *TAG = "DISCORD_TASKS";

// Function to clear log file
esp_err_t clear_discord_logs(void) {
    const char* LOG_FILE = "/spiffs/debug_log.txt";
    ESP_LOGI(TAG, "Clearing log file");
    
    if (unlink(LOG_FILE) == 0) {
        ESP_LOGI(TAG, "Log file cleared successfully");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to clear log file");
        return ESP_FAIL;
    }
}

// Discord message sending task structure
typedef struct {
    char message[4096]; 
    SemaphoreHandle_t done_semaphore;
    esp_err_t result;
} discord_task_data_t;

// Task for sending Discord messages with a larger stack
static void discord_send_task(void *pvParameters) {
    discord_task_data_t *task_data = (discord_task_data_t *)pvParameters;
    
    ESP_LOGI(TAG, "Discord send task started, message length: %d", strlen(task_data->message));
    
    // Send the message
    task_data->result = discord_send_message(task_data->message);
    
    // Signal completion
    xSemaphoreGive(task_data->done_semaphore);
    vTaskDelete(NULL);
}

// Function to send Discord message using a separate task
esp_err_t discord_send_message_safe(const char *message) {
    // Allocate memory for task data structure
    discord_task_data_t *task_data = malloc(sizeof(discord_task_data_t));
    if (task_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for task data");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Creating Discord send task");
    
    // Create semaphore for synchronization
    task_data->done_semaphore = xSemaphoreCreateBinary();
    if (task_data->done_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        free(task_data);
        return ESP_ERR_NO_MEM;
    }
    
    // Copy message to task data
    size_t msg_len = strlen(message);
    size_t max_copy = sizeof(task_data->message) - 1;
    
    if (msg_len > max_copy) {
        ESP_LOGW(TAG, "Message length (%zu) exceeds buffer size (%zu), will be truncated", 
                  msg_len, max_copy);
    }
    
    strncpy(task_data->message, message, max_copy);
    task_data->message[max_copy] = '\0'; // Ensure null terminator
    
    // Create task with larger stack
    BaseType_t task_created = xTaskCreate(
        discord_send_task,
        "discord_send_task",
        12288,  
        task_data,  // Pass pointer to allocated memory
        5,     // Priority
        NULL
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Discord send task");
        vSemaphoreDelete(task_data->done_semaphore);
        free(task_data);
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Waiting for Discord send task to complete");
    
    // Wait for task completion
    esp_err_t result;
    if (xSemaphoreTake(task_data->done_semaphore, pdMS_TO_TICKS(30000)) != pdTRUE) {
        ESP_LOGE(TAG, "Discord send task timeout");
        // Don't free task_data as the task might still be using it
        vSemaphoreDelete(task_data->done_semaphore);
        return ESP_ERR_TIMEOUT;
    }
    
    // Save the result
    result = task_data->result;
    
    ESP_LOGI(TAG, "Discord send task completed with result: %s", 
             result == ESP_OK ? "OK" : "Failed");
    
    // Clean up
    vSemaphoreDelete(task_data->done_semaphore);
    free(task_data);
    
    return result;
}

// Function to send measurements with retries using a separate task
esp_err_t send_measurements_with_task_retries(const char* measurements, int max_retries) {
    esp_err_t ret = ESP_FAIL;
    
    for (int i = 0; i < max_retries; i++) {
        // Sending measurements using task
        ret = discord_send_message_safe(measurements);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGI(TAG, "Failed to send measurements, retrying (%d/%d)", i+1, max_retries);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGE(TAG, "All measurement send retries failed");
    return ret;
}

// Function to send logs with retries using a separate task
esp_err_t send_logs_with_task_retries(int max_retries) {
    esp_err_t ret = ESP_FAIL;
    
    // Logs receiving - using the external function from storage component
    extern char* storage_get_logs(void);
    char* logs = storage_get_logs();
    if (logs == NULL) {
        ESP_LOGW(TAG, "No logs to send (NULL returned)");
        return ESP_OK; 
    }
    
    ESP_LOGI(TAG, "Retrieved logs: length=%d bytes", strlen(logs));
    
    for (int i = 0; i < max_retries; i++) {
        // Sending logs using task 
        ret = discord_send_message_safe(logs);
        if (ret == ESP_OK) {
            // Clearing the log file after successful sending
            ESP_LOGI(TAG, "Successfully sent logs");
            ret = clear_discord_logs();
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to clear logs after successful sending");
            }
            free(logs);
            return ESP_OK;
        }
        
        ESP_LOGI(TAG, "Failed to send logs, retrying (%d/%d)", i+1, max_retries);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGE(TAG, "Failed to send logs after %d retries", max_retries);
    free(logs);
    return ret;
}
