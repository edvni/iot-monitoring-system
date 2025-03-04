#include "discord_api.h"
#include "discord_cert.h"
#include "storage.h"
#include "esp_http_client.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_tls.h"

static const char *TAG = "discord_api";
#define ESP_TLS_VER_TLS_1_2 0x0303 /* TLS 1.2 */
#define ESP_TLS_VER_TLS_1_3 0x0304 /* TLS 1.3 */

static struct {
    const char* bot_token{nullptr};
    const char* channel_id{nullptr};
} s_config;

static esp_err_t discord_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADERS_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADERS_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA");
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

esp_err_t discord_init(const discord_config_t* config) {
    if (config == NULL || config->bot_token == NULL || config->channel_id == NULL) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    s_config.bot_token = config->bot_token;
    s_config.channel_id = config->channel_id;

    ESP_LOGI(TAG, "Discord API initialized for channel %s", config->channel_id);
    return ESP_OK;
}

esp_err_t discord_send_message(const char *message) {
    if (s_config.bot_token == NULL || s_config.channel_id == NULL) {
        ESP_LOGE(TAG, "Discord API not initialized inside discord_send_message");
        return ESP_ERR_INVALID_STATE;
    }

    // URL construction for API endpoint
    char url[128];
    snprintf(url, sizeof(url), "https://discord.com/api/v10/channels/%s/messages", s_config.channel_id);

    ESP_LOGI(TAG, "Trying to connect to URL: %s", url);

    // Creating JSON object with test message
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "content", message);
    char *post_data = cJSON_PrintUnformatted(root);

    // HTTP client configuration
    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.cert_pem = discord_root_cert;
    config.event_handler = discord_http_event_handler;
    config.timeout_ms = 10000;
    config.buffer_size = 2048;
    config.port = 443;  // HTTPS port
    config.tls_version = ESP_HTTP_CLIENT_TLS_VER_ANY;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(post_data);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // Headers setup
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bot %s", s_config.bot_token);
    esp_http_client_set_header(client, "Authorization", auth_header);

    
    // POST request sending
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);
        
        if (status_code == 200 || status_code == 204) {
            ESP_LOGI(TAG, "Message sent successfully");
        } else {
            ESP_LOGE(TAG, "Failed to send message, status code: %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    // Freeing up resources
    free(post_data);
    cJSON_Delete(root);
    esp_http_client_cleanup(client);
    
    return err;
}

esp_err_t send_measurements_with_retries(const char* measurements, int max_retries) {
    esp_err_t ret = ESP_FAIL;
    
    for (int i = 0; i < max_retries; i++) {
        // Sending measurements
        ret = discord_send_message(measurements);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        //storage_append_log("Failed to send measurements, retrying");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    //storage_append_log("All measurement send retries failed");
    return ret;
}

esp_err_t send_logs_with_retries(int max_retries) {
    esp_err_t ret = ESP_FAIL;
    
    // Logs receiving
    char* logs = storage_get_logs();
    if (logs == NULL) {
        return ESP_OK; 
    }
    
    for (int i = 0; i < max_retries; i++) {
   
        ret = discord_send_message(logs);
        if (ret == ESP_OK) {
            // Clearing the log file after successful sending
            unlink("/spiffs/debug_log.txt");
            free(logs);
            return ESP_OK;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    //storage_append_log("Failed to send logs");
    free(logs);
    return ret;
}

