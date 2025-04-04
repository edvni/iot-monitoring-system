#pragma once

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Format initial message with timestamp and battery information
 * 
 * @param message Buffer to store the formatted message
 * @param message_size Size of the message buffer
 * @return esp_err_t ESP_OK on success
 */
esp_err_t reporter_format_initial_message(char *message, size_t message_size);

#ifdef __cplusplus
}
#endif 