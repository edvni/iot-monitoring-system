#include "time_manager.h"
#include "gsm_modem.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "TIME_MANAGER";

// Set Finland timezone (UTC+2 standard, UTC+3 DST)
#define TIMEZONE_OFFSET_STANDARD 7200  // UTC+2 in seconds
#define TIMEZONE_OFFSET_DST      10800 // UTC+3 in seconds

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

time_t parse_timestamp_for_firebase(const char *timestamp_str) {
    struct tm tm = {0};
    if (strptime(timestamp_str, "%Y-%m-%d %H:%M:%S", &tm) == NULL) {
        ESP_LOGE(TAG, "Failed to parse timestamp string: %s", timestamp_str);
        return (time_t)-1;
    }

    // Manually adjust for Finland timezone (DST handled below)
    tm.tm_isdst = -1; // Let mktime determine if DST is in effect
    time_t local_time = mktime(&tm);

    // Convert local time to UTC
    struct tm *utc_tm = gmtime(&local_time);
    time_t utc_time = mktime(utc_tm);

    // Apply timezone offset (reverse of mktime's local-to-UTC conversion)
    time_t final_time = local_time + (local_time - utc_time);

    // Finland DST runs from last sunday March to last Sunday October
    time_t march_last_sunday = mktime(&tm) + (TIMEZONE_OFFSET_DST - TIMEZONE_OFFSET_STANDARD);
    time_t october_last_sunday = mktime(&tm) - (TIMEZONE_OFFSET_DST - TIMEZONE_OFFSET_STANDARD);

    if (final_time >= march_last_sunday && final_time < october_last_sunday) {
        final_time -= TIMEZONE_OFFSET_DST; // Apply DST offset
    } else {
        final_time -= TIMEZONE_OFFSET_STANDARD; // Apply standard offset
    }

    return final_time;
}
    
