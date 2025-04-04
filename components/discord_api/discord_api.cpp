#include "discord_api.h"
#include "discord_cert.h"
#include "discord_config.h"
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

static const discord_config_t config = {
    .bot_token = DISCORD_BOT_TOKEN,
    .channel_id = DISCORD_CHANNEL_ID
};

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

esp_err_t discord_init(void) {
    ESP_LOGI(TAG, "Initializing Discord API for channel %s", config.channel_id);
    
    s_config.bot_token = config.bot_token;
    s_config.channel_id = config.channel_id;

    return ESP_OK;
}

esp_err_t discord_send_message(const char *message) {
    if (s_config.bot_token == NULL || s_config.channel_id == NULL) {
        ESP_LOGE(TAG, "Discord API not initialized inside discord_send_message");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Check incoming message
    if (message == NULL) {
        ESP_LOGE(TAG, "Message is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(message) == 0) {
        ESP_LOGE(TAG, "Message is empty");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Note: Discord has a 2000 character limit for messages
    // For longer messages use send_large_message_in_chunks in discord_tasks.c
    size_t msg_len = strlen(message);
    if (msg_len > 2000) {
        ESP_LOGW(TAG, "Message length (%d) exceeds Discord limit of 2000 characters. It may be truncated.", 
                 msg_len);
    }

    // URL construction for API endpoint
    char url[128];
    snprintf(url, sizeof(url), "https://discord.com/api/v10/channels/%s/messages", s_config.channel_id);

    ESP_LOGI(TAG, "Trying to connect to URL: %s", url);

    // Creating JSON object with test message
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(root, "content", message);
    if (cJSON_GetObjectItem(root, "content") == NULL) {
        ESP_LOGE(TAG, "Failed to add message to JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    char *post_data = cJSON_PrintUnformatted(root);
    size_t post_data_len = 0;
    
    if (post_data == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON string");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    post_data_len = strlen(post_data);
    ESP_LOGI(TAG, "JSON data length: %d", post_data_len);

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

    // Headers setup - protection from errors when setting headers
    esp_err_t header_err = ESP_OK;
    header_err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (header_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Content-Type header: %s", esp_err_to_name(header_err));
        free(post_data);
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bot %s", s_config.bot_token);
    header_err = esp_http_client_set_header(client, "Authorization", auth_header);
    if (header_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Authorization header: %s", esp_err_to_name(header_err));
        free(post_data);
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // POST request sending with additional check
    esp_err_t post_err = esp_http_client_set_post_field(client, post_data, post_data_len);
    if (post_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set post field: %s", esp_err_to_name(post_err));
        free(post_data);
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    // Performing the request with exception protection
    esp_err_t err;
    try {
        err = esp_http_client_perform(client);
    } catch (...) {
        ESP_LOGE(TAG, "Exception occurred during HTTP request");
        free(post_data);
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

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



