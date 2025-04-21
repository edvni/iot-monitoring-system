#include "firebase_api.h"
#include "firebase_config.h"
#include "storage.h"
#include "jwt_util.h"
#include "firebase_cert.h"
#include "time_manager.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>  
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "lwip/sockets.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"

#define ESP_TLS_VER_TLS_1_2 0x0303 /* TLS 1.2 */
#define ESP_TLS_VER_TLS_1_3 0x0304 /* TLS 1.3 */
#define HTTP_RX_BUFFER_SIZE 8192   
#define HTTP_TX_BUFFER_SIZE 13824  
#define MAX_HTTP_RETRIES 3         // Maximum number of retry attempts

// Буфер для разбивки JSON на части
#define STREAM_CHUNK_SIZE 2048
static char stream_buffer[STREAM_CHUNK_SIZE];
static const char *current_json_data = NULL;
static size_t json_data_len = 0;
static size_t json_data_pos = 0;

static const char *TAG = "firebase_api";

// JWT token and expiration time
static char jwt_token[2048] = {0};
static int64_t token_expiration_time = 0;

// Function prototypes to allow to be called before their definitions, static for internal use only
static esp_err_t create_jwt_token(void);
static bool is_token_valid(void);
static esp_err_t firebase_http_event_handler(esp_http_client_event_t *evt);
//static char* format_firestore_data(const char *json_str);

// Create a JWT token for Firebase authentication
static esp_err_t create_jwt_token(void) {
    time_t now = 0;
    time(&now);

    // Token will expire in 1 hour
    token_expiration_time = now + 3600;

    // Generate JWT using the utility function in jwt_util.h
    generate_jwt(now);

    // Copy the generated JWT to our token buffer
    strncpy(jwt_token, (char*)jwt, sizeof(jwt_token) - 1);
    jwt_token[sizeof(jwt_token) - 1] = '\0';

    ESP_LOGI(TAG, "JWT token created successfully");
    return ESP_OK;
}

// Check if token needs refresh
static bool is_token_valid(void) {
    time_t now;
    time(&now);
    
    // Refresh if token is expired or about to expire in 5 minutes
    return (token_expiration_time != 0 && now <= (token_expiration_time - 300));
}

// HTTP event handler
static esp_err_t firebase_http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP Connected to server");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP Header Sent");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP Data received, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP Event Finished");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP Disconnected");
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Public function implementations

esp_err_t firebase_init(void) {
    // Check if time is set
    time_t now = 0;
    time(&now);

    if (now < 1600000000) {
        ESP_LOGE(TAG, "Time not set correctly. Please configure SNTP first.");
        return ESP_FAIL;
    }

    // Enable TLS debug logging
    esp_log_level_set("esp-tls", ESP_LOG_DEBUG);
    esp_log_level_set("esp-tls-mbedtls", ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "TLS debug logging enabled");

    // Generate initial JWT token
    esp_err_t err = create_jwt_token();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create initial JWT token");
        return err;
    }

    ESP_LOGI(TAG, "Firebase initialized successfully");
    return ESP_OK;
}


// Send pre-formatted Firestore data (without formatting, since data is already in Firestore format)
// esp_err_t firebase_send_firestore_data(const char *collection, const char *document_id, const char *firestore_data) {
//     // Check parameters
//     if (!collection || !firestore_data) {
//         return ESP_ERR_INVALID_ARG;
//     }

//     // Log input data size
//     ESP_LOGI(TAG, "Input Firestore data size: %zu bytes", strlen(firestore_data));
//     ESP_LOGI(TAG, "Current heap: %" PRIu32 " bytes free", (uint32_t)esp_get_free_heap_size());
    
//     // Check and refresh token if needed
//     if (!is_token_valid()) {
//         ESP_LOGI(TAG, "Token expired or about to expire, generating new token");
//         if (create_jwt_token() != ESP_OK) {
//             ESP_LOGE(TAG, "Failed to create new JWT token");
//             return ESP_FAIL;
//         }
//     }

//     // Format URL
//     char url[256];
//     if (document_id != NULL && strlen(document_id) > 0) {
//         // For first request no need for updateMask - send full document
//         // Parameter currentDocument.exists=false guarantees that document will be created if it doesn't exist
//         snprintf(url, sizeof(url), "%s/%s/%s?currentDocument.exists=false", 
//                 FIREBASE_URL, collection, document_id);
//     } else {
//         // Create document with auto-generated ID
//         snprintf(url, sizeof(url), "%s/%s", FIREBASE_URL, collection);
//     }
    
//     ESP_LOGI(TAG, "Sending Firestore data to URL: %s", url);

//     // Select HTTP method depending on presence of document_id
//     esp_http_client_method_t http_method;
//     if (document_id != NULL && strlen(document_id) > 0) {
//         // For document with specified ID use PATCH (creates or updates)
//         http_method = HTTP_METHOD_PATCH;
//         ESP_LOGI(TAG, "Using HTTP PATCH for document with specified ID");
//     } else {
//         // For auto-generated ID use POST
//         http_method = HTTP_METHOD_POST;
//         ESP_LOGI(TAG, "Using HTTP POST for auto-generated document ID");
//     }

//     // Configure HTTP client with increased buffer sizes
//     esp_http_client_config_t config = {
//         .url = url,
//         .event_handler = firebase_http_event_handler,
//         .method = http_method,  
//         .transport_type = HTTP_TRANSPORT_OVER_SSL,
//         .cert_pem = firebase_root_cert,
//         .buffer_size = HTTP_RX_BUFFER_SIZE,     
//         .buffer_size_tx = HTTP_TX_BUFFER_SIZE,  
//         .timeout_ms = 30000,                    
//         .keep_alive_enable = true,              
//         .skip_cert_common_name_check = true,    
//         .port = 443,
//         .use_global_ca_store = false,           
//         .crt_bundle_attach = esp_crt_bundle_attach
//     };

//     ESP_LOGI(TAG, "HTTP client config - RX buffer: %d, TX buffer: %d", 
//              HTTP_RX_BUFFER_SIZE, HTTP_TX_BUFFER_SIZE);
//     ESP_LOGI(TAG, "Memory before HTTP client init: free heap: %" PRIu32 ", largest block: %" PRIu32,
//              (uint32_t)esp_get_free_heap_size(), 
//              (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

//     esp_http_client_handle_t client = esp_http_client_init(&config);
//     if (client == NULL) {
//         ESP_LOGE(TAG, "Failed to initialize HTTP client");
//         return ESP_FAIL;
//     }

//     // Set headers
//     size_t auth_header_size = strlen("Bearer ") + strlen(jwt_token) + 1;
//     char *auth_header = malloc(auth_header_size);
//     if (auth_header == NULL) {
//         ESP_LOGE(TAG, "Failed to allocate memory for auth header");
//         esp_http_client_cleanup(client);
//         return ESP_ERR_NO_MEM;
//     }
    
//     snprintf(auth_header, auth_header_size, "Bearer %s", jwt_token);
//     esp_http_client_set_header(client, "Authorization", auth_header);
//     esp_http_client_set_header(client, "Content-Type", "application/json");

//     // Set request body - since firestore_data is constant, we shouldn't free it
//     esp_http_client_set_post_field(client, firestore_data, strlen(firestore_data));

//     // Perform HTTP request with retry mechanism
//     esp_err_t err = ESP_FAIL;
//     for (int retry = 0; retry < MAX_HTTP_RETRIES; retry++) {
//         err = esp_http_client_perform(client);
        
//         if (err == ESP_OK) {
//             break;
//         }
        
//         ESP_LOGW(TAG, "HTTP request failed (attempt %d/%d): %s", 
//                  retry + 1, MAX_HTTP_RETRIES, esp_err_to_name(err));
        
//         if (retry < MAX_HTTP_RETRIES - 1) {
//             // Wait before next attempt (increasing waiting time)
//             int delay_ms = (retry + 1) * 1000;
//             ESP_LOGI(TAG, "Retrying in %d ms...", delay_ms);
//             vTaskDelay(pdMS_TO_TICKS(delay_ms));
//         }
//     }
    
//     // Free memory auth_header
//     free(auth_header);
    
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "HTTP POST request failed after %d attempts: %s", 
//                 MAX_HTTP_RETRIES, esp_err_to_name(err));
//         esp_http_client_cleanup(client);
//         return err;
//     }

//     int status_code = esp_http_client_get_status_code(client);
//     ESP_LOGI(TAG, "HTTP status code: %d", status_code);

//     // For code 400, try to get response body with error
//     if (status_code >= 400) {
//         // Get response body
//         int content_length = esp_http_client_get_content_length(client);
//         ESP_LOGI(TAG, "Error response length: %d", content_length);
        
//         if (content_length > 0 && content_length < 2048) {
//             char *response_buffer = malloc(content_length + 1);
//             if (response_buffer) {
//                 int read_len = esp_http_client_read_response(client, response_buffer, content_length);
//                 if (read_len > 0) {
//                     response_buffer[read_len] = 0; // Null-terminate
//                     ESP_LOGE(TAG, "Error response: %s", response_buffer);
//                 }
//                 free(response_buffer);
//             }
//         }
//     }

//     // Check response
//     if (status_code == 200 || status_code == 201) {
//         ESP_LOGI(TAG, "Firebase data sent successfully");
        
//         // Get successful response body (for debugging)
//         int content_length = esp_http_client_get_content_length(client);
//         if (content_length > 0 && content_length < 2048) {
//             char *response_buffer = malloc(content_length + 1);
//             if (response_buffer) {
//                 int read_len = esp_http_client_read_response(client, response_buffer, content_length);
//                 if (read_len > 0) {
//                     response_buffer[read_len] = 0; // Null-terminate
//                     ESP_LOGI(TAG, "Success response (first 200 chars): %.200s%s", 
//                             response_buffer, strlen(response_buffer) > 200 ? "..." : "");
//                 }
//                 free(response_buffer);
//             }
//         }
//     } else {
//         ESP_LOGE(TAG, "Failed to send data. Status code: %d", status_code);
//         err = ESP_FAIL;
//     }

//     esp_http_client_cleanup(client);
//     return (status_code == 200 || status_code == 201) ? ESP_OK : ESP_FAIL;
// }




// Simplified version without data provider
esp_err_t firebase_send_streamed_data(const char *collection, const char *document_id, const char *firestore_data) {
    // Check arguments
    if (!collection || !firestore_data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // JSON data size
    size_t data_size = strlen(firestore_data);
    ESP_LOGI(TAG, "Total data size: %zu bytes", data_size);
    
    // Check token
    if (!is_token_valid()) {
        if (create_jwt_token() != ESP_OK) {
            return ESP_FAIL;
        }
    }
    
    // Forming URL
    char url[256];
    if (document_id && strlen(document_id) > 0) {
        snprintf(url, sizeof(url), "%s/%s/%s", FIREBASE_URL, collection, document_id);
    } else {
        snprintf(url, sizeof(url), "%s/%s", FIREBASE_URL, collection);
    }
    
    // HTTP method
    esp_http_client_method_t http_method = (document_id && strlen(document_id) > 0) ? 
                                         HTTP_METHOD_PATCH : HTTP_METHOD_POST;
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = firebase_http_event_handler,
        .method = http_method,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = firebase_root_cert,
        .buffer_size = 4096,        
        .buffer_size_tx = 4096,     
        .timeout_ms = 60000,        
        .crt_bundle_attach = esp_crt_bundle_attach
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;
    
    // Allocate memory for auth_header in heap
    size_t auth_header_size = strlen("Bearer ") + strlen(jwt_token) + 1;
    char *auth_header = malloc(auth_header_size);
    if (!auth_header) {
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    snprintf(auth_header, auth_header_size, "Bearer %s", jwt_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    // Setting data directly
    esp_http_client_set_post_field(client, firestore_data, data_size);
    
    // Request
    esp_err_t err = esp_http_client_perform(client);
    int status_code = (err == ESP_OK) ? esp_http_client_get_status_code(client) : 0;
    
    ESP_LOGI(TAG, "HTTP status: %d, result: %s", 
             status_code, (err == ESP_OK) ? "OK" : esp_err_to_name(err));
    
    // Free memory
    free(auth_header);
    esp_http_client_cleanup(client);
    
    return (status_code == 200 || status_code == 201) ? ESP_OK : ESP_FAIL;
}
