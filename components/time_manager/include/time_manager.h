#pragma once

#include "esp_err.h"
#include <time.h>

/**
 * @brief Set timezone for Finland (EET with DST)
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t time_manager_set_finland_timezone(void);


/**
 * @brief Get current time as formatted string
 * 
 * @param buffer Buffer to store formatted time string
 * @param buffer_size Size of the buffer
 * @return esp_err_t ESP_OK on success
 */
esp_err_t time_manager_get_formatted_time(char *buffer, size_t buffer_size);

/**
 * @brief Set time from network timestamp (UTC)
 * 
 * @param timestamp UTC timestamp from NTP
 * @return esp_err_t ESP_OK on success
 */
esp_err_t time_manager_set_from_timestamp(time_t timestamp);

/**
 * @brief Parse timestamp string to time_t for firebase
 * 
 * @param timestamp_str Timestamp string in format "YYYY-MM-DD HH:MM:SS"
 * @return time_t Parsed timestamp
 */
time_t parse_timestamp_for_firebase(const char *timestamp_str);





