#include "time_manager.h"
#include "gsm_modem.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "TIME_MANAGER";

esp_err_t time_manager_set_finland_timezone(void) {
    ESP_LOGI(TAG, "Setting timezone to EET (UTC+2) with DST (UTC+3)");
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1); // EET (UTC+2) with DST (UTC+3)
    tzset(); // Apply the timezone settings
    return ESP_OK;
}


esp_err_t time_manager_get_formatted_time(char *buffer, size_t buffer_size) {
    // Ensure timezone is set before getting formatted time
    time_manager_set_finland_timezone();
    
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &timeinfo) == 0) {
        ESP_LOGE(TAG, "Failed to format time string");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t time_manager_set_from_timestamp(time_t timestamp) {
    if (timestamp <= 0) {
        ESP_LOGE(TAG, "Invalid timestamp for time synchronization");
        return ESP_ERR_INVALID_ARG;
    }

    // Make sure timezone is set
    time_manager_set_finland_timezone();

    // Set system time (UTC)
    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0;
    
    esp_err_t ret = settimeofday(&tv, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set time from timestamp");
        return ret;
    }
    
    // Verify the time setting
    char time_str[64];
    if (time_manager_get_formatted_time(time_str, sizeof(time_str)) == ESP_OK) {
        ESP_LOGI(TAG, "Time set from timestamp. Current time: %s", time_str);
    }
    
    return ESP_OK;
}

