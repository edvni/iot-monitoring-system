#pragma once

#include "esp_err.h"

/**
 * @brief Initialize time manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t time_manager_init(void);

/**
 * @brief Get current time as formatted string
 * 
 * @param buffer Buffer to store formatted time string
 * @param buffer_size Size of the buffer
 * @return esp_err_t ESP_OK on success
 */
esp_err_t time_manager_get_formatted_time(char *buffer, size_t buffer_size);

/**
 * @brief Set current time manually
 * 
 * @param year Year (e.g., 2024)
 * @param month Month (1-12)
 * @param day Day (1-31)
 * @param hour Hour (0-23)
 * @param minute Minute (0-59)
 * @param second Second (0-59)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t time_manager_set_time(int year, int month, int day, 
                               int hour, int minute, int second);

/**
 * @brief Synchronize system time with network time servers
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t time_manager_sync_network_time(void);

/**
 * @brief Save current time before going to sleep
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t time_manager_before_sleep(void);


