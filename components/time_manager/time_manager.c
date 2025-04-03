#include "time_manager.h"
#include "gsm_modem.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "TIME_MANAGER";

esp_err_t time_manager_init(void) {
    // Need to set initial time to something
    struct tm timeinfo = {
        .tm_year = 2024 - 1900,   // Years in tm struct start from 1900
        .tm_mon = 1,              
        .tm_mday = 1,            
        .tm_hour = 0,
        .tm_min = 0,
        .tm_sec = 0
    };
    struct timeval tv = {
        .tv_sec = mktime(&timeinfo)
    };
    
    esp_err_t ret = settimeofday(&tv, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set initial time");
        return ret;
    }
    
    ESP_LOGI(TAG, "Time manager initialized");
    return ESP_OK;
}

esp_err_t time_manager_get_formatted_time(char *buffer, size_t buffer_size) {
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

esp_err_t time_manager_set_time(int year, int month, int day, 
                               int hour, int minute, int second) {
    struct tm timeinfo = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,  // Need to convert from 1-12 to 0-11 because tm_mon is 0-11
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = second
    };
    
    struct timeval tv = {
        .tv_sec = mktime(&timeinfo)
    };
    
    esp_err_t ret = settimeofday(&tv, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set time");
        return ret;
    }
    
    ESP_LOGI(TAG, "Time set to: %04d-%02d-%02d %02d:%02d:%02d", 
             year, month, day, hour, minute, second);
    return ESP_OK;
}
