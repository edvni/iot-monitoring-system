#ifndef DISCORD_API_H
#define DISCORD_API_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Discord API data structure
 */
typedef struct {
    const char* bot_token; // Bot token
    const char* channel_id; // Channel ID
} discord_config_t;

/**
 * @brief Initialize Discord API with default configuration
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t discord_init(void);

/**
 * @brief Send message to Discord channel
 * 
 * @param message Message to send
 * @return esp_err_t ESP_OK on success
 */
esp_err_t discord_send_message(const char *message);

/**
 * @brief Send message to Discord channel using a separate task with larger stack
 * 
 * @param message Message to send
 * @return esp_err_t ESP_OK on success
 */
esp_err_t discord_send_message_safe(const char *message);

/**
 * @brief Send initial message with system information to Discord
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t send_initial_discord_message(void);

/**
 * @brief Send measurements to Discord channel using a separate task with larger stack
 * 
 * @param measurements Measurements to send
 * @param max_retries Maximum number of retries
 * @return esp_err_t ESP_OK on success
 */
esp_err_t send_measurements_with_task_retries(const char* measurements, int max_retries);

/**
 * @brief Send logs to Discord channel using a separate task with larger stack
 * 
 * @param max_retries Maximum number of retries
 * @return esp_err_t ESP_OK on success
 */
esp_err_t send_logs_with_task_retries(int max_retries);

/**
 * @brief Clear Discord log file
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t clear_discord_logs(void);

#ifdef __cplusplus
}
#endif

#endif /* DISCORD_API_H */