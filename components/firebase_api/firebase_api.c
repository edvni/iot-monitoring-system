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
#define HTTP_RX_BUFFER_SIZE 8192    // Increased from 2048
#define HTTP_TX_BUFFER_SIZE 17408   // Increased from 4096 to 64K
#define MAX_HTTP_RETRIES 3         // Максимальное количество повторных попыток

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

    // Включаем отладочный вывод TLS
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

// // Send data to Firestore
// esp_err_t firebase_send_data(const char *collection, const char *document_id, const char *json_data) {
//     // Check parameters
//     if (!collection || !json_data) {
//         return ESP_ERR_INVALID_ARG;
//     }

//     // Log input data size
//     ESP_LOGI(TAG, "Input JSON size: %d bytes", strlen(json_data));
//     ESP_LOGI(TAG, "Current heap: %" PRIu32 " bytes free", esp_get_free_heap_size());

//     // Format data for Firestore
//     char *firestore_data = format_firestore_data(json_data);
//     if (firestore_data == NULL) {
//         ESP_LOGE(TAG, "Failed to format data for Firestore");
//         return ESP_FAIL;
//     }
    
//     // Check and refresh token if needed
//     if (!is_token_valid()) {
//         ESP_LOGI(TAG, "Token expired or about to expire, generating new token");
//         if (create_jwt_token() != ESP_OK) {
//             ESP_LOGE(TAG, "Failed to create new JWT token");
//             free(firestore_data);
//             return ESP_FAIL;
//         }
//     }

//     // Format URL
//     char url[256];
//     if (document_id != NULL && strlen(document_id) > 0) {
//         // Update/create specific document
//         snprintf(url, sizeof(url), "%s/%s/%s", FIREBASE_URL, collection, document_id);
//     } else {
//         // Create document with auto-generated ID
//         snprintf(url, sizeof(url), "%s/%s", FIREBASE_URL, collection);
//     }

//     // Configure HTTP client with increased buffer sizes
//     esp_http_client_config_t config = {
//         .url = url,
//         .event_handler = firebase_http_event_handler,
//         .method = HTTP_METHOD_POST,
//         .transport_type = HTTP_TRANSPORT_OVER_SSL,
//         .cert_pem = firebase_root_cert,
//         .buffer_size = HTTP_RX_BUFFER_SIZE,     // Increased buffer size
//         .buffer_size_tx = HTTP_TX_BUFFER_SIZE,  // Increased buffer size
//         .timeout_ms = 60000,                    // Увеличиваем таймаут до 60 секунд
//         .keep_alive_enable = true,              // Включаем keep-alive
//         .skip_cert_common_name_check = true,    // Пропускаем проверку общего имени в сертификате
//         .port = 443,
//     };

//     ESP_LOGI(TAG, "Using buffer sizes - RX: %d, TX: %d", HTTP_RX_BUFFER_SIZE, HTTP_TX_BUFFER_SIZE);
    
//     esp_http_client_handle_t client = esp_http_client_init(&config);
//     if (!client) {
//         ESP_LOGE(TAG, "Failed to initialize HTTP client");
//         free(firestore_data);
//         return ESP_FAIL;
//     }

//     // Set headers
//     // Используем динамическое выделение памяти для auth_header
//     size_t auth_header_size = strlen("Bearer ") + strlen(jwt_token) + 1;
//     char *auth_header = malloc(auth_header_size);
//     if (auth_header == NULL) {
//         ESP_LOGE(TAG, "Failed to allocate memory for auth header");
//         free(firestore_data);
//         esp_http_client_cleanup(client);
//         return ESP_ERR_NO_MEM;
//     }
    
//     snprintf(auth_header, auth_header_size, "Bearer %s", jwt_token);
//     esp_http_client_set_header(client, "Authorization", auth_header);
//     esp_http_client_set_header(client, "Content-Type", "application/json");

//     // Set request body
//     esp_http_client_set_post_field(client, firestore_data, strlen(firestore_data));

//     // Perform HTTP request with retry mechanism
//     esp_err_t err = ESP_FAIL;
//     for (int retry = 0; retry < MAX_HTTP_RETRIES; retry++) {
//         err = esp_http_client_perform(client);
        
//         if (err == ESP_OK) {
//             // Успешно, выходим из цикла
//             break;
//         }
        
//         ESP_LOGW(TAG, "HTTP request failed (attempt %d/%d): %s", 
//                  retry + 1, MAX_HTTP_RETRIES, esp_err_to_name(err));
        
//         if (retry < MAX_HTTP_RETRIES - 1) {
//             // Ждем перед следующей попыткой (увеличивая время ожидания)
//             int delay_ms = (retry + 1) * 1000;
//             ESP_LOGI(TAG, "Retrying in %d ms...", delay_ms);
//             vTaskDelay(pdMS_TO_TICKS(delay_ms));
//         }
//     }
    
//     // Освобождаем память auth_header
//     free(auth_header);
    
//     if (err == ESP_OK) {
//         int status_code = esp_http_client_get_status_code(client);
//         ESP_LOGI(TAG, "HTTP status code: %d", status_code);

//         if (status_code < 200 || status_code >= 300) {
//             ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
//             err = ESP_FAIL;
//         }
//     } else {
//         ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
//     }

//     // Clean up
//     esp_http_client_cleanup(client);
//     free(firestore_data);
    
//     return err;
// }

// Send pre-formatted Firestore data (без форматирования, т.к. данные уже в формате Firestore)
esp_err_t firebase_send_firestore_data(const char *collection, const char *document_id, const char *firestore_data) {
    // Check parameters
    if (!collection || !firestore_data) {
        return ESP_ERR_INVALID_ARG;
    }

    // Log input data size
    ESP_LOGI(TAG, "Input Firestore data size: %zu bytes", strlen(firestore_data));
    ESP_LOGI(TAG, "Current heap: %" PRIu32 " bytes free", (uint32_t)esp_get_free_heap_size());
    
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
        // Для первого запроса не нужен updateMask - отправляем полный документ
        // Параметр currentDocument.exists=false гарантирует, что документ будет создан, если его нет
        snprintf(url, sizeof(url), "%s/%s/%s?currentDocument.exists=false", 
                FIREBASE_URL, collection, document_id);
    } else {
        // Create document with auto-generated ID
        snprintf(url, sizeof(url), "%s/%s", FIREBASE_URL, collection);
    }
    
    ESP_LOGI(TAG, "Sending Firestore data to URL: %s", url);

    // Выбираем HTTP метод в зависимости от presence document_id
    esp_http_client_method_t http_method;
    if (document_id != NULL && strlen(document_id) > 0) {
        // Для документа с указанным ID используем PATCH (создает или обновляет)
        http_method = HTTP_METHOD_PATCH;
        ESP_LOGI(TAG, "Using HTTP PATCH for document with specified ID");
    } else {
        // Для автоматически генерируемого ID используем POST
        http_method = HTTP_METHOD_POST;
        ESP_LOGI(TAG, "Using HTTP POST for auto-generated document ID");
    }

    // Configure HTTP client with increased buffer sizes
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = firebase_http_event_handler,
        .method = http_method,  // Используем выбранный метод
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = firebase_root_cert,
        .buffer_size = HTTP_RX_BUFFER_SIZE,     // Increased buffer size
        .buffer_size_tx = HTTP_TX_BUFFER_SIZE,  // Increased buffer size
        .timeout_ms = 30000,                    // Таймаут 30 секунд
        .keep_alive_enable = true,              // Включаем keep-alive
        .skip_cert_common_name_check = true,    // Пропускаем проверку общего имени в сертификате
        .port = 443,
        .use_global_ca_store = false,           // Экономит память
        .crt_bundle_attach = esp_crt_bundle_attach
    };

    ESP_LOGI(TAG, "HTTP client config - RX buffer: %d, TX buffer: %d", 
             HTTP_RX_BUFFER_SIZE, HTTP_TX_BUFFER_SIZE);
    ESP_LOGI(TAG, "Memory before HTTP client init: free heap: %" PRIu32 ", largest block: %" PRIu32,
             (uint32_t)esp_get_free_heap_size(), 
             (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    // Set headers
    size_t auth_header_size = strlen("Bearer ") + strlen(jwt_token) + 1;
    char *auth_header = malloc(auth_header_size);
    if (auth_header == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for auth header");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    snprintf(auth_header, auth_header_size, "Bearer %s", jwt_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Set request body - поскольку firestore_data константный, мы не должны его освобождать
    esp_http_client_set_post_field(client, firestore_data, strlen(firestore_data));

    // Perform HTTP request with retry mechanism
    esp_err_t err = ESP_FAIL;
    for (int retry = 0; retry < MAX_HTTP_RETRIES; retry++) {
        err = esp_http_client_perform(client);
        
        if (err == ESP_OK) {
            // Успешно, выходим из цикла
            break;
        }
        
        ESP_LOGW(TAG, "HTTP request failed (attempt %d/%d): %s", 
                 retry + 1, MAX_HTTP_RETRIES, esp_err_to_name(err));
        
        if (retry < MAX_HTTP_RETRIES - 1) {
            // Ждем перед следующей попыткой (увеличивая время ожидания)
            int delay_ms = (retry + 1) * 1000;
            ESP_LOGI(TAG, "Retrying in %d ms...", delay_ms);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
    
    // Освобождаем память auth_header
    free(auth_header);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST request failed after %d attempts: %s", 
                MAX_HTTP_RETRIES, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP status code: %d", status_code);

    // Для кода 400 пытаемся получить тело ответа с ошибкой
    if (status_code >= 400) {
        // Получаем тело ответа
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "Error response length: %d", content_length);
        
        if (content_length > 0 && content_length < 2048) {
            char *response_buffer = malloc(content_length + 1);
            if (response_buffer) {
                int read_len = esp_http_client_read_response(client, response_buffer, content_length);
                if (read_len > 0) {
                    response_buffer[read_len] = 0; // Null-terminate
                    ESP_LOGE(TAG, "Error response: %s", response_buffer);
                }
                free(response_buffer);
            }
        }
    }

    // Check response
    if (status_code == 200 || status_code == 201) {
        ESP_LOGI(TAG, "Firebase data sent successfully");
        
        // Получаем тело успешного ответа (для отладки)
        int content_length = esp_http_client_get_content_length(client);
        if (content_length > 0 && content_length < 2048) {
            char *response_buffer = malloc(content_length + 1);
            if (response_buffer) {
                int read_len = esp_http_client_read_response(client, response_buffer, content_length);
                if (read_len > 0) {
                    response_buffer[read_len] = 0; // Null-terminate
                    ESP_LOGI(TAG, "Success response (first 200 chars): %.200s%s", 
                            response_buffer, strlen(response_buffer) > 200 ? "..." : "");
                }
                free(response_buffer);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to send data. Status code: %d", status_code);
        err = ESP_FAIL;
    }

    esp_http_client_cleanup(client);
    return (status_code == 200 || status_code == 201) ? ESP_OK : ESP_FAIL;
}

// Вспомогательная функция для отправки по указанному URL с настройками merge
static esp_err_t firebase_send_to_url(const char *full_url, const char *data) {
    // Check parameters
    if (!full_url || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    // Log input data size
    ESP_LOGI(TAG, "Input data size: %zu bytes", strlen(data));
    ESP_LOGI(TAG, "Current heap: %" PRIu32 " bytes free", (uint32_t)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Sending to URL: %s", full_url);
    
    // Check and refresh token if needed
    if (!is_token_valid()) {
        ESP_LOGI(TAG, "Token expired or about to expire, generating new token");
        if (create_jwt_token() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create new JWT token");
            return ESP_FAIL;
        }
    }

    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = full_url,
        .event_handler = firebase_http_event_handler,
        .method = HTTP_METHOD_PATCH,  // Используем PATCH для merge
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = firebase_root_cert,
        .buffer_size = HTTP_RX_BUFFER_SIZE,
        .buffer_size_tx = HTTP_TX_BUFFER_SIZE,
        .timeout_ms = 30000,                    // Таймаут 30 секунд
        .keep_alive_enable = true,              // Включаем keep-alive
        .skip_cert_common_name_check = true,    // Пропускаем проверку общего имени в сертификате
        .port = 443,
        .use_global_ca_store = false,           // Экономит память
        .crt_bundle_attach = esp_crt_bundle_attach
    };

    ESP_LOGI(TAG, "HTTP client config - RX buffer: %d, TX buffer: %d", 
             HTTP_RX_BUFFER_SIZE, HTTP_TX_BUFFER_SIZE);
    ESP_LOGI(TAG, "Memory before HTTP client init: free heap: %" PRIu32 ", largest block: %" PRIu32,
             (uint32_t)esp_get_free_heap_size(), 
             (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    // Set headers
    size_t auth_header_size = strlen("Bearer ") + strlen(jwt_token) + 1;
    char *auth_header = malloc(auth_header_size);
    if (auth_header == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for auth header");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    snprintf(auth_header, auth_header_size, "Bearer %s", jwt_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Set request body
    esp_http_client_set_post_field(client, data, strlen(data));

    // Perform HTTP request with retry mechanism
    esp_err_t err = ESP_FAIL;
    for (int retry = 0; retry < MAX_HTTP_RETRIES; retry++) {
        err = esp_http_client_perform(client);
        
        if (err == ESP_OK) {
            // Успешно, выходим из цикла
            break;
        }
        
        ESP_LOGW(TAG, "HTTP PATCH request failed (attempt %d/%d): %s", 
                 retry + 1, MAX_HTTP_RETRIES, esp_err_to_name(err));
        
        if (retry < MAX_HTTP_RETRIES - 1) {
            // Ждем перед следующей попыткой
            int delay_ms = (retry + 1) * 1000;
            ESP_LOGI(TAG, "Retrying in %d ms...", delay_ms);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
    
    // Освобождаем память auth_header
    free(auth_header);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP PATCH request failed after %d attempts: %s", 
                MAX_HTTP_RETRIES, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP status code: %d", status_code);

    // Для кода 400 пытаемся получить тело ответа с ошибкой
    if (status_code >= 400) {
        // Получаем тело ответа
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "Error response length: %d", content_length);
        
        if (content_length > 0 && content_length < 2048) {
            char *response_buffer = malloc(content_length + 1);
            if (response_buffer) {
                int read_len = esp_http_client_read_response(client, response_buffer, content_length);
                if (read_len > 0) {
                    response_buffer[read_len] = 0; // Null-terminate
                    ESP_LOGE(TAG, "Error response: %s", response_buffer);
                }
                free(response_buffer);
            }
        }
    }

    // Check response
    if (status_code == 200 || status_code == 201) {
        ESP_LOGI(TAG, "Firebase data merged successfully");
        
        // Получаем тело успешного ответа (для отладки)
        int content_length = esp_http_client_get_content_length(client);
        if (content_length > 0 && content_length < 2048) {
            char *response_buffer = malloc(content_length + 1);
            if (response_buffer) {
                int read_len = esp_http_client_read_response(client, response_buffer, content_length);
                if (read_len > 0) {
                    response_buffer[read_len] = 0; // Null-terminate
                    ESP_LOGI(TAG, "Success response (first 200 chars): %.200s%s", 
                            response_buffer, strlen(response_buffer) > 200 ? "..." : "");
                }
                free(response_buffer);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to merge data. Status code: %d", status_code);
        err = ESP_FAIL;
    }

    esp_http_client_cleanup(client);
    return (status_code == 200 || status_code == 201) ? ESP_OK : ESP_FAIL;
}

// esp_err_t firebase_send_chunked_data(const char *collection, const char *document_id, const char *firestore_data) {
//     ESP_LOGI(TAG, "Firebase Chunked Data: collection=%s, document_id=%s", 
//              collection, document_id ? document_id : "NULL (auto-generated)");
    
//     // Парсим данные
//     cJSON *doc = cJSON_Parse(firestore_data);

//     if (!doc) {
//         ESP_LOGE(TAG, "Failed to parse Firestore data");
//         return ESP_FAIL;
//     }
    
//     // Найдем массив measurements
//     cJSON *fields = cJSON_GetObjectItem(doc, "fields");
//     cJSON *measurements = cJSON_GetObjectItem(fields, "measurements");
//     cJSON *array_value = cJSON_GetObjectItem(measurements, "arrayValue");
//     cJSON *values = cJSON_GetObjectItem(array_value, "values");
    
//     if (!values || !cJSON_IsArray(values)) {
//         ESP_LOGE(TAG, "Invalid measurements format");
//         cJSON_Delete(doc);
//         return ESP_FAIL;
//     }
    
//     // Определяем общее количество измерений
//     int total_items = cJSON_GetArraySize(values);
//     ESP_LOGI(TAG, "Total measurements: %d", total_items);
    
//     // Создаем документ с метаданными (без измерений)
//     cJSON *metadata_doc = cJSON_CreateObject();
//     cJSON *metadata_fields = cJSON_CreateObject();
    
//     // Копируем все поля кроме measurements из исходного документа
//     cJSON *item = fields->child;
//     while (item) {
//         if (strcmp(item->string, "measurements") != 0) {
//             cJSON_AddItemToObject(metadata_fields, item->string, cJSON_Duplicate(item, 1));
//         }
//         item = item->next;
//     }
    
//     // Добавляем поля в итоговый документ
//     cJSON_AddItemToObject(metadata_doc, "fields", metadata_fields);
    
//     // Получаем JSON-строку для метаданных
//     char *metadata_str = cJSON_PrintUnformatted(metadata_doc);
//     cJSON_Delete(metadata_doc);
    
//     if (metadata_str == NULL) {
//         ESP_LOGE(TAG, "Failed to create JSON for metadata");
//         cJSON_Delete(doc);
//         return ESP_FAIL;
//     }
    
//     ESP_LOGI(TAG, "Metadata chunk (first 200 chars): %.200s%s", 
//              metadata_str, strlen(metadata_str) > 200 ? "..." : "");
    
//     ESP_LOGI(TAG, "Free heap before JSON: %" PRIu32, (uint32_t)esp_get_free_heap_size());
//     // Отправляем метаданные (первая часть)
//     ESP_LOGI(TAG, "Sending metadata with document_id=%s", 
//             document_id ? document_id : "NULL (auto-generated)");
//     esp_err_t metadata_result = firebase_send_firestore_data(collection, document_id, metadata_str);
//     free(metadata_str);
//     ESP_LOGI(TAG, "Free heap after JSON: %" PRIu32, (uint32_t)esp_get_free_heap_size());
    
//     if (metadata_result != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to send metadata: %s", esp_err_to_name(metadata_result));
//         cJSON_Delete(doc);
//         return metadata_result;
//     }
    
//     // Делаем паузу перед вторым запросом
//     vTaskDelay(pdMS_TO_TICKS(1000));
    
//     // Теперь отправим все измерения одним запросом
//     // Убедимся, что document_id не NULL для второго запроса
//     if (!document_id || strlen(document_id) == 0) {
//         ESP_LOGE(TAG, "Cannot send measurements without document_id");
//         cJSON_Delete(doc);
//         return ESP_FAIL;
//     }
    
//     // Формируем URL для обновления только поля measurements
//     char measurements_url[300];
//     snprintf(measurements_url, sizeof(measurements_url), 
//             "%s/%s/%s?updateMask.fieldPaths=measurements&currentDocument.exists=true", 
//             FIREBASE_URL, collection, document_id);
    
//     ESP_LOGI(TAG, "Sending all measurements to URL: %s", measurements_url);
    
//     // Создаем итоговый документ только с измерениями
//     cJSON *meas_doc = cJSON_CreateObject();
//     cJSON *meas_fields = cJSON_CreateObject();
    
//     // Добавляем поле measurements
//     cJSON_AddItemToObject(meas_fields, "measurements", cJSON_Duplicate(measurements, 1));
//     cJSON_AddItemToObject(meas_doc, "fields", meas_fields);
    
//     // Сериализуем документ с измерениями
//     char *meas_str = cJSON_PrintUnformatted(meas_doc);
//     cJSON_Delete(meas_doc);
    
//     if (meas_str == NULL) {
//         ESP_LOGE(TAG, "Failed to create JSON for measurements");
//         cJSON_Delete(doc);
//         return ESP_FAIL;
//     }
    
//     ESP_LOGI(TAG, "Measurements data (first 200 chars): %.200s%s", 
//              meas_str, strlen(meas_str) > 200 ? "..." : "");
    
//     size_t meas_size = strlen(meas_str);
//     ESP_LOGI(TAG, "Exact measurements size: %zu bytes", meas_size);
    
//     if (meas_size > HTTP_TX_BUFFER_SIZE - 1024) { // Оставляем запас 1KB для заголовков
//         ESP_LOGW(TAG, "Warning: Measurements data size (%zu bytes) is close to buffer limit (%d bytes)",
//                 meas_size, (int)HTTP_TX_BUFFER_SIZE);
//     }
    
//     // Отправляем все измерения
//     esp_err_t meas_result = firebase_send_to_url(measurements_url, meas_str);
//     free(meas_str);
    
//     if (meas_result != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to send measurements: %s", esp_err_to_name(meas_result));
//         cJSON_Delete(doc);
//         return meas_result;
//     }
    
//     // Очищаем ресурсы
//     cJSON_Delete(doc);
    
//     ESP_LOGI(TAG, "Largest free block: %" PRIu32, (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    
//     return ESP_OK;
// }


