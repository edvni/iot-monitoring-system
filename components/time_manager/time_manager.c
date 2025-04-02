#include "time_manager.h"
#include "gsm_modem.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"    
#include "freertos/task.h"        
#include <time.h>
#include <sys/time.h>
#include "esp_sleep.h"
#include "esp_timer.h"  

static const char *TAG = "TIME_MANAGER";

RTC_DATA_ATTR static time_t rtc_time_at_sleep = 0;
RTC_DATA_ATTR static uint64_t sleep_time_us = 0;

esp_err_t time_manager_init(void) {
    struct timeval tv;

    if (rtc_time_at_sleep > 0) {
        // Restore time after waking up
        uint64_t sleep_duration = (esp_timer_get_time() - sleep_time_us);
        time_t current_time = rtc_time_at_sleep + (sleep_duration / 1000000);
        
        tv.tv_sec = current_time;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "Time restored after sleep");
    } else {
        // Initial time initialization
        tv.tv_sec = 1712076000;  // 2024-04-03 12:00:00 UTC
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "Initial time set");
    }

    // Set timezone for Finland
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();

    return ESP_OK;
}

esp_err_t time_manager_before_sleep(void) {
    time(&rtc_time_at_sleep);
    sleep_time_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Time saved before sleep");
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

esp_err_t time_manager_sync_network_time(void) {
    // Initialize SNTP with default configuration
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    // Wait for sync
    int retry = 0;
    const int retry_count = 10;
    
    while (retry < retry_count) {
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) == ESP_OK) {
            break;
        }
        ESP_LOGI(TAG, "Waiting for time sync... (%d/%d)", ++retry, retry_count);
    }

    if (retry == retry_count) {
        ESP_LOGE(TAG, "Time sync failed");
        return ESP_FAIL;
    }

    char time_str[64];
    if (time_manager_get_formatted_time(time_str, sizeof(time_str)) == ESP_OK) {
        ESP_LOGI(TAG, "Time synchronized: %s", time_str);
    }
    
    return ESP_OK;
}