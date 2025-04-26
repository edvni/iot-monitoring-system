#include "firebase_api.h"
#include "firebase_config.h"
#include "storage.h"
#include "jwt_util.h"
#include "firebase_cert.h"
#include "time_manager.h"
#include "json_helper.h"

/**
 * @file firebase_api.c
 * @brief Implementation of the Firebase API with optimized memory usage
 * 
 * This module uses dynamic memory allocation in the heap (heap) instead of
 * local buffers on the stack to prevent stack overflow when processing data
 * from multiple sensors. The main principles:
 * 
 * 1. All large buffers are allocated through heap_caps_malloc() with subsequent freeing
 * 2. After each HTTP operation, a pause is inserted to release resources
 * 3. Buffers are freed as soon as possible after use
 */

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
    
    // Forming URL - using heap
    char *url = heap_caps_malloc(256, MALLOC_CAP_8BIT);
    if (!url) {
        ESP_LOGE(TAG, "Failed to allocate memory for URL");
        return ESP_ERR_NO_MEM;
    }
    
    if (document_id && strlen(document_id) > 0) {
        snprintf(url, 256, "%s/%s/%s", FIREBASE_URL, collection, document_id);
    } else {
        snprintf(url, 256, "%s/%s", FIREBASE_URL, collection);
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
    if (!client) {
        free(url);
        return ESP_FAIL;
    }
    
    // Allocate memory for auth_header in heap
    size_t auth_header_size = strlen("Bearer ") + strlen(jwt_token) + 1;
    char *auth_header = heap_caps_malloc(auth_header_size, MALLOC_CAP_8BIT);
    if (!auth_header) {
        esp_http_client_cleanup(client);
        free(url);
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
    free(url);
    esp_http_client_cleanup(client);
    
    return (status_code == 200 || status_code == 201) ? ESP_OK : ESP_FAIL;
}

// Reading the content of a file into a buffer that has already been allocated in memory
static esp_err_t read_sensor_file_to_buffer(const char *file_path, char *buffer, size_t buffer_size) {
    if (!file_path || !buffer || buffer_size == 0) {
        ESP_LOGE(TAG, "Invalid arguments for reading file");
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE *f = fopen(file_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open sensor file: %s", file_path);
        return ESP_FAIL;
    }
    
    // Getting file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0) {
        ESP_LOGE(TAG, "Invalid file size: %ld", file_size);
        fclose(f);
        return ESP_FAIL;
    }
    
    if ((size_t)file_size >= buffer_size) {
        ESP_LOGE(TAG, "File size (%ld) exceeds buffer size (%zu)", file_size, buffer_size);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    // Reading file to buffer
    size_t bytes_read = fread(buffer, 1, file_size, f);
    fclose(f);
    
    if (bytes_read != (size_t)file_size) {
        ESP_LOGE(TAG, "Failed to read complete file. Read %zu of %ld bytes", bytes_read, file_size);
        return ESP_FAIL;
    }
    
    // Adding terminating zero
    buffer[bytes_read] = '\0';
    
    ESP_LOGI(TAG, "Retrieved Firestore data, size: %zu bytes", bytes_read);
    ESP_LOGI(TAG, "Current heap: %" PRIu32 " bytes free", esp_get_free_heap_size());
    
    return ESP_OK;
}

// Processing and sending data from sensor
static esp_err_t process_and_send_sensor_data(const char *json_data) {
    if (!json_data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Allocating memory on heap for MAC address
    char *mac_address = heap_caps_malloc(18, MALLOC_CAP_8BIT);
    if (!mac_address) {
        ESP_LOGE(TAG, "Failed to allocate memory for MAC address");
        return ESP_ERR_NO_MEM;
    }
    
    // Extracting MAC address from JSON using helper function
    esp_err_t ret = json_helper_extract_mac_address(json_data, mac_address, 18);
    if (ret != ESP_OK) {
        free(mac_address);
        return ret;
    }
    
    ESP_LOGI(TAG, "Extracted MAC address: %s", mac_address);
    
    // Getting current date for creating document ID
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    // Allocating memory on heap for time_str
    char *time_str = heap_caps_malloc(20, MALLOC_CAP_8BIT);
    if (!time_str) {
        ESP_LOGE(TAG, "Failed to allocate memory for time string");
        free(mac_address);
        return ESP_ERR_NO_MEM;
    }
    
    strftime(time_str, 20, "%Y-%m-%d", &timeinfo);
    
    // Formatting MAC address (replacing colons with underscores)
    char *formatted_mac = heap_caps_malloc(18, MALLOC_CAP_8BIT);
    if (!formatted_mac) {
        ESP_LOGE(TAG, "Failed to allocate memory for formatted MAC");
        free(time_str);
        free(mac_address);
        return ESP_ERR_NO_MEM;
    }
    
    json_helper_format_mac_address(mac_address, formatted_mac, 18);
    
    // Generating document ID
    char *document_id = heap_caps_malloc(64, MALLOC_CAP_8BIT);
    if (!document_id) {
        ESP_LOGE(TAG, "Failed to allocate memory for document ID");
        free(formatted_mac);
        free(time_str);
        free(mac_address);
        return ESP_ERR_NO_MEM;
    }
    
    json_helper_generate_document_id(time_str, formatted_mac, document_id, 64);
    
    ESP_LOGI(TAG, "Using custom document ID: %s", document_id);
    
    // Sending data to Firestore
    ret = firebase_send_streamed_data("daily_measurements", document_id, json_data);
    
    // Freeing allocated memory
    free(document_id);
    free(formatted_mac);
    free(time_str);
    free(mac_address);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Measurements sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send measurements");
    }    
    return ret;
}

esp_err_t send_all_sensor_measurements_to_firebase(void) {
    static const char *TAG_FIREBASE = "FIREBASE_TASKS";
    
    // Getting list of files
    char **file_list = NULL;
    int file_count = 0;
    esp_err_t ret = storage_get_sensor_files(&file_list, &file_count);
    
    if (ret != ESP_OK || file_count == 0 || file_list == NULL) {
        ESP_LOGE(TAG_FIREBASE, "No sensor files found");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG_FIREBASE, "Found %d sensor files to send", file_count);
    
    // Variables for tracking results
    bool any_success = false;
    bool any_failure = false;
    int success_count = 0;
    
    // Processing each file separately with pauses between requests
    for (int i = 0; i < file_count; i++) {
        ESP_LOGI(TAG_FIREBASE, "Processing file %d/%d: %s", i+1, file_count, file_list[i]);
        
        // Allocating memory for file content
        char *file_content = heap_caps_malloc(18 * 1024, MALLOC_CAP_8BIT);
        if (!file_content) {
            ESP_LOGE(TAG_FIREBASE, "Failed to allocate memory for file content");
            any_failure = true;
            // Continuing to process other files
            continue;
        }
        
        // Reading file to allocated memory
        ret = read_sensor_file_to_buffer(file_list[i], file_content, 18 * 1024);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_FIREBASE, "Failed to read file: %s", file_list[i]);
            free(file_content);
            any_failure = true;
            continue;
        }
        
        // Processing and sending data
        ret = process_and_send_sensor_data(file_content);
        
        // Freeing file content memory immediately after use
        free(file_content);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG_FIREBASE, "Successfully sent data from file %s", file_list[i]);
            any_success = true;
            success_count++;
            
            // Deleting file after successful sending
            if (unlink(file_list[i]) != 0) {
                ESP_LOGW(TAG_FIREBASE, "Failed to delete file after successful upload: %s", file_list[i]);
            } else {
                ESP_LOGI(TAG_FIREBASE, "Deleted file after successful upload: %s", file_list[i]);
            }
        } else {
            ESP_LOGE(TAG_FIREBASE, "Failed to send data from file %s", file_list[i]);
            any_failure = true;
        }
        
        // Pause between sending gives time to release resources
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Freeing list of files
    storage_free_sensor_files(file_list, file_count);
    
    // Logging results
    ESP_LOGI(TAG_FIREBASE, "Sensor data upload summary: %d/%d files successfully sent", 
             success_count, file_count);
    
    // Defining return status
    if (any_success && !any_failure) {
        return ESP_OK;              // All successfully sent
    } else if (any_success) {
        return ESP_ERR_INVALID_STATE; // Some sent, some not
    } else {
        return ESP_FAIL;            // None sent
    }
}
