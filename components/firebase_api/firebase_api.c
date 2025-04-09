#include "firebase_api.h"
#include "firebase_config.h"
#include "storage.h"
#include "jwt_util.h"
#include "firebase_cert.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "lwip/sockets.h"

#define ESP_TLS_VER_TLS_1_2 0x0303 /* TLS 1.2 */
#define ESP_TLS_VER_TLS_1_3 0x0304 /* TLS 1.3 */

static const char *TAG = "firebase_api";

// JWT token and expiration  time
static char jwt_token[2048] = {0};
static int64_t token_expiration_time = 0;

// Function prototypes
static esp_err_t create_jwt_token(void);
static bool is_token_valid(void);
static esp_err_t firebase_http_event_handler(esp_http_client_event_t *evt);
static char* format_firestore_data(const char *json_str);

typedef struct {
    char tag_id[18]; // MAC address of the RuuviTag
    int num_samples;
} mock_data_task_params_t;

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

    ESP_LOGI(TAG, "JWT token created succesfully: %s", jwt_token);
    return ESP_OK;
}

// Check if token needs refresh
static bool is_token_valid(void) {
    time_t now;
    time(&now);
    
    // Refresh if token is expired or about to expire in 5 minutes
    if (token_expiration_time == 0 || now > (token_expiration_time - 300)) {
        return false;
    }
    return true;
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
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP Header: %s: %s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP Data received, len=%d", evt->data_len);
            if (evt->data_len > 0) {
                ESP_LOGD(TAG, "Data: %.*s", evt->data_len, (char*)evt->data);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP Event Finished");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP Disconnected from server");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP Redirect");
            break;
    }
    return ESP_OK;
}

// Format Firestore document data
static char* format_firestore_data(const char *json_str) {
    cJSON *root = cJSON_CreateObject();
    cJSON *fields = cJSON_CreateObject();

    // Parse the input JSON
    cJSON *input_json = cJSON_Parse(json_str);
    if (input_json == NULL || !cJSON_IsObject(input_json)) {
        ESP_LOGE(TAG, "Invalid JSON input or not an object: %s", json_str);
        ESP_LOGE(TAG, "Failed to parse JSON input");
        cJSON_Delete(root);
        return NULL;
    }

    // Convert each field to Firestore format
    cJSON *item = input_json->child;
    while (item != NULL) {
        cJSON *value_obj = cJSON_CreateObject();

        // Determine field type and format accordingly
        if (cJSON_IsString(item)) {
            cJSON_AddStringToObject(value_obj, "stringValue", item->valuestring);
        } else if (cJSON_IsNumber(item)) {
            if (strcmp(item->string, "timestamp") == 0) {
                // Convert Unix time to ISO 8601
                time_t raw_time = (time_t)item->valuedouble;
                struct tm *timeinfo = gmtime(&raw_time);
                char timestamp_str[40];
                strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%S", timeinfo);
                strcat(timestamp_str, ".000Z"); // Append milliseconds and Zulu time
                cJSON_AddStringToObject(value_obj, "timestampValue", timestamp_str);
            } else if (item->valuedouble == (double)(int)item->valuedouble) {
                cJSON_AddNumberToObject(value_obj, "integerValue", (int)item->valuedouble);
            } else {
                cJSON_AddNumberToObject(value_obj, "doubleValue", item->valuedouble);
            }
        } else if (cJSON_IsBool(item)) {
            cJSON_AddBoolToObject(value_obj, "booleanValue", item->valueint ? 1 : 0);
        } else if (cJSON_IsNull(item)) {
            cJSON_AddNullToObject(value_obj, "nullValue");
        }

        cJSON_AddItemToObject(fields, item->string, value_obj);
        item = item->next;
    }

    cJSON_AddItemToObject(root, "fields", fields);
    char *formatted_data = cJSON_PrintUnformatted(root);

    // Clean up
    cJSON_Delete(input_json);
    cJSON_Delete(root);

    return formatted_data;
}

// Pulic function implementations

esp_err_t firebase_init(void) {
    
    // Initialize time (needed for token generation)
    time_t now = 0;
    time(&now);

    if (now < 1600000000) {
        ESP_LOGE(TAG, "Time not set correctly. Please configure SNTP first.");
        return ESP_FAIL;
    }

    // generate initial JWT token
    esp_err_t err = create_jwt_token();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create initial JWT token");
        return err;
    }

    ESP_LOGI(TAG, "Firebase initialized successfully");
    return ESP_OK;
}

esp_err_t firebase_send_data(const char *collection, const char *document_id, const char *json_data) {
    
    // Check and refresh token if needed
    if (!is_token_valid()) {
        ESP_LOGI(TAG, "Token expired or about to expire, generating new token");
        if (create_jwt_token() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create new JWT token");
            return ESP_FAIL;
        }
    }

    // Format URL
    char url[256];
    if (document_id != NULL && strlen(document_id) > 0) {
        // Update/create specific document
        snprintf(url, sizeof(url), "%s/%s/%s", FIREBASE_URL, collection, document_id);
        ESP_LOGI(TAG, "FIRESTORE_URL: %s", FIREBASE_URL);
        ESP_LOGI(TAG, "Collection: %s", collection);
        ESP_LOGI(TAG, "Document ID: %s", document_id ? document_id : "NULL");
    } else {
        // Create document with auto-generated ID
        snprintf(url, sizeof(url), "%s/%s", FIREBASE_URL, collection);
        ESP_LOGI(TAG, "FIRESTORE_URL: %s", FIREBASE_URL);
        ESP_LOGI(TAG, "Collection: %s", collection);
        ESP_LOGI(TAG, "Document ID: %s", document_id ? document_id : "NULL");
    }

    // Format data for Firestore
    char *firestore_data = format_firestore_data(json_data);
    if (firestore_data == NULL) {
        ESP_LOGE(TAG, "Failed to format data for Firestore");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Firestore URL: %s", url);

    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = firebase_http_event_handler,
        //.method = document_id ? HTTP_METHOD_PATCH : HTTP_METHOD_POST,
        .method = HTTP_METHOD_POST, // Use POST for both creating and updating documents
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = firebase_root_cert,
        .buffer_size = 2048,
        .buffer_size_tx = 4096,
        .timeout_ms = 10000,
        .port = 443, // HTTPS port
        .tls_version = ESP_HTTP_CLIENT_TLS_VER_ANY,
    };

    ESP_LOGI(TAG, "Initializing HTTP client, using Firebase root certificate");
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set headers
    char auth_header[4096];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", jwt_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Set request body
    esp_http_client_set_post_field(client, firestore_data, strlen(firestore_data));

    ESP_LOGI(TAG, "Request body: %s", firestore_data);

    // Perform HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP request status = %d", status_code);
        if (status_code != 200 && status_code != 201) {
            // Read response body to get error details
            int content_length = esp_http_client_get_content_length(client);
            if (content_length > 0) {
                char *response_buffer = malloc(content_length + 1);
                if (response_buffer) {
                    int read_len = esp_http_client_read(client, response_buffer, content_length);
                    response_buffer[read_len] = '\0'; // Null-terminate the response
                    ESP_LOGE(TAG, "Error response: %s", response_buffer);
                    free(response_buffer);
                }
            }
        } else if (status_code == 200 && status_code <= 299) {
            ESP_LOGI(TAG, "Data sent successfully to Firestore");
        } else {
            ESP_LOGE(TAG, "Failed to send data to Firestore, status code: %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    // Clean up
    esp_http_client_cleanup(client);
    free(firestore_data);
    return err;
}

/**
 * @brief Send mock sensor data to Firestore for testing connection
 * 
 * @param tag_id A mock identifier for the sensor
 * @param num_samples Number of mock readings to send
 * @return esp_err_t ESP_OK if all data was sent successfully, otherwise ESP_FAIL
 */
 esp_err_t firebase_send_mock_data(const char *tag_id, int num_samples) {
    esp_err_t result = ESP_OK;
    time_t now;
    time(&now);

    ESP_LOGI(TAG, "Sending %d mock samples for tag %s", num_samples, tag_id);

    // initialize firebase if not already done
    if (!is_token_valid()) {
        ESP_LOGI(TAG, "TEST Token expired or about to expire, generating new token");
        if (firebase_init() != ESP_OK) {
            ESP_LOGE(TAG, "TEST Failed to initialize Firebase for mock data sending");
            return ESP_FAIL;
        }
    }

    // Generate and send multiple samples with slightly different values
    for (int i = 0; i < num_samples; i++) {
        // Generate mock sensor values with some variation
        float mock_temperature = 22.5f + ((float)esp_random() / UINT32_MAX) * 5.0f; // 22.5-27.5°C
        float mock_humidity = 45.0f + ((float)esp_random() / UINT32_MAX) * 20.0f;  // 45-65%
        
        ESP_LOGI(TAG, "Mock sample %d: temp=%.2f°C, humidity=%.2f%%", 
                i+1, mock_temperature, mock_humidity);
        
        // Create a simple JSON string directly instead of using firebase_send_ruuvitag_data
        // Reduces stack usage by avoiding nested function calls
        char json_data[150];
        snprintf(json_data, sizeof(json_data), 
            "{\"tag_id\":\"%s\",\"temperature\":%.2f,\"humidity\":%.2f,\"timestamp\":%ld}",
            tag_id, mock_temperature, mock_humidity, (long)(now + i));
        
        // Send directly to the measurements collection with auto-generated document ID
        esp_err_t err = firebase_send_data("measurements", NULL, json_data);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send mock sample %d", i+1);
            result = ESP_FAIL;
            // Continue trying other samples even if one fails
        }
        
        // Add a small delay between sends to avoid overwhelming the server
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    return result;
 }

 // Task that sends mock data
static void mock_data_sender_task(void *pvParameters) {
    mock_data_task_params_t *params = (mock_data_task_params_t *)pvParameters;
    
    ESP_LOGI(TAG, "Mock data sender task started");
    
    // Call the function to send mock data
    esp_err_t result = firebase_send_mock_data(params->tag_id, params->num_samples);
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Mock data test completed successfully");
    } else {
        ESP_LOGE(TAG, "Mock data test failed");
    }
    
    // Free the parameters
    free(params);
    
    // Delete the task
    vTaskDelete(NULL);
}

/**
 * @brief Start a task to send mock data to Firebase
 * 
 * @param tag_id A mock identifier for the sensor
 * @param num_samples Number of mock readings to send
 * @return esp_err_t ESP_OK if task started successfully, ESP_FAIL otherwise
 */
 esp_err_t firebase_start_mock_data_task(const char *tag_id, int num_samples) {
    // Allocate parameters
    mock_data_task_params_t *params = malloc(sizeof(mock_data_task_params_t));
    if (params == NULL) {
        ESP_LOGE(TAG, "Failed to allocate task parameters");
        return ESP_FAIL;
    }
    
    // Copy parameters
    strlcpy(params->tag_id, tag_id, sizeof(params->tag_id));
    params->num_samples = num_samples;
    
    // Create the task with a larger stack size
    TaskHandle_t task_handle = NULL;
    BaseType_t result = xTaskCreate(
        mock_data_sender_task,
        "mock_data_sender",
        8192,  // Increase stack size to 8KB (default is often 4KB)
        params,
        5,     // Priority (higher than lowest)
        &task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create mock data sender task");
        free(params);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Mock data sender task created successfully");
    return ESP_OK;
}