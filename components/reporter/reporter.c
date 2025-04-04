#include "reporter.h"
#include "esp_log.h"
#include "battery_monitor.h"
#include <time.h>
#include <string.h>

static const char *TAG = "reporter";

esp_err_t reporter_format_initial_message(char *message, size_t message_size) {
    if (message == NULL || message_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize message with current date and time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char time_str[40];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    snprintf(message, message_size - 1, "%s - Measurements started", time_str);
    message[message_size - 1] = '\0';  // Ensure null terminator
    
    // Add battery information to the message
    battery_info_t battery_info;
    if (battery_monitor_read(&battery_info) == ESP_OK) {
        // Limit the length of the main message to avoid buffer overflow
        if (message_size > 90) {
            message[90] = '\0'; // Truncate message to 90 characters for guaranteed safety
        }

        // Use snprintf for better safety
        char temp[200]; // Increase buffer size
        snprintf(temp, sizeof(temp), "%s - Battery: %lu mV, Level: %d%%", 
                 message, battery_info.voltage_mv, battery_info.level);
        
        // Copy back to message with length limitation
        strncpy(message, temp, message_size - 1);
        message[message_size - 1] = '\0'; // Ensure null terminator
    }
    
    ESP_LOGI(TAG, "Formatted initial message: %s", message);
    return ESP_OK;
} 