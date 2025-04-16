#include "firebase_api.h"
#include "firebase_config.h"
#include "storage.h"
#include "jwt_util.h"
#include "firebase_cert.h"
#include "time_manager.h"

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

// JWT token and expiration time
static char jwt_token[2048] = {0};
static int64_t token_expiration_time = 0;

// Function prototypes to allow to be called before their definitions, static for internal use only
static esp_err_t create_jwt_token(void);
static bool is_token_valid(void);
static esp_err_t firebase_http_event_handler(esp_http_client_event_t *evt);
static char* format_firestore_data(const char *json_str);

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

// Format Firestore document data
static char* format_firestore_data(const char *json_str) {
    cJSON *root = cJSON_CreateObject();
    cJSON *fields = cJSON_CreateObject();

    // Parse the input JSON
    cJSON *input_json = cJSON_Parse(json_str);
    if (input_json == NULL || !cJSON_IsObject(input_json)) {
        ESP_LOGE(TAG, "Invalid JSON input");
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
        } else if (cJSON_IsArray(item)) {
            // Обработка массивов для Firestore
            cJSON *array_value = cJSON_CreateObject();
            cJSON *values_array = cJSON_CreateArray();
            
            // Итерируемся по элементам массива
            cJSON *array_item = item->child;
            while (array_item != NULL) {
                // Если это объект в массиве
                if (cJSON_IsObject(array_item)) {
                    // Создаем структуру mapValue для каждого элемента массива
                    cJSON *map_obj = cJSON_CreateObject();
                    cJSON *map_value = cJSON_CreateObject();
                    cJSON *map_fields = cJSON_CreateObject();
                    
                    // Итерируемся по полям этого объекта
                    cJSON *field = array_item->child;
                    while (field != NULL) {
                        cJSON *field_value_obj = cJSON_CreateObject();
                        
                        // Форматируем поле в зависимости от типа
                        if (cJSON_IsString(field)) {
                            cJSON_AddStringToObject(field_value_obj, "stringValue", field->valuestring);
                        } else if (cJSON_IsNumber(field)) {
                            if (field->valuedouble == (double)(int)field->valuedouble) {
                                cJSON_AddNumberToObject(field_value_obj, "integerValue", (int)field->valuedouble);
                            } else {
                                cJSON_AddNumberToObject(field_value_obj, "doubleValue", field->valuedouble);
                            }
                        } else if (cJSON_IsBool(field)) {
                            cJSON_AddBoolToObject(field_value_obj, "booleanValue", field->valueint ? 1 : 0);
                        } else if (cJSON_IsNull(field)) {
                            cJSON_AddNullToObject(field_value_obj, "nullValue");
                        }
                        
                        // Добавляем поле в объект полей
                        cJSON_AddItemToObject(map_fields, field->string, field_value_obj);
                        field = field->next;
                    }
                    
                    // Добавляем объект полей в mapValue
                    cJSON_AddItemToObject(map_value, "fields", map_fields);
                    // Добавляем mapValue в объект
                    cJSON_AddItemToObject(map_obj, "mapValue", map_value);
                    // Добавляем объект в массив значений
                    cJSON_AddItemToArray(values_array, map_obj);
                }
                
                array_item = array_item->next;
            }
            
            // Добавляем массив значений в структуру для Firestore
            cJSON_AddItemToObject(array_value, "arrayValue", cJSON_CreateObject());
            cJSON_AddItemToObject(cJSON_GetObjectItem(array_value, "arrayValue"), "values", values_array);
            
            // Заменяем value_obj на array_value
            cJSON_Delete(value_obj);
            value_obj = array_value;
        } else if (cJSON_IsObject(item)) {
            // Обработка вложенных объектов для Firestore
            cJSON *map_value = cJSON_CreateObject();
            cJSON *map_fields = cJSON_CreateObject();
            
            // Итерируемся по полям вложенного объекта
            cJSON *obj_item = item->child;
            while (obj_item != NULL) {
                cJSON *field_value_obj = cJSON_CreateObject();
                
                // Форматируем поле в зависимости от типа
                if (cJSON_IsString(obj_item)) {
                    cJSON_AddStringToObject(field_value_obj, "stringValue", obj_item->valuestring);
                } else if (cJSON_IsNumber(obj_item)) {
                    if (obj_item->valuedouble == (double)(int)obj_item->valuedouble) {
                        cJSON_AddNumberToObject(field_value_obj, "integerValue", (int)obj_item->valuedouble);
                    } else {
                        cJSON_AddNumberToObject(field_value_obj, "doubleValue", obj_item->valuedouble);
                    }
                } else if (cJSON_IsBool(obj_item)) {
                    cJSON_AddBoolToObject(field_value_obj, "booleanValue", obj_item->valueint ? 1 : 0);
                } else if (cJSON_IsNull(obj_item)) {
                    cJSON_AddNullToObject(field_value_obj, "nullValue");
                }
                
                // Добавляем поле в объект полей
                cJSON_AddItemToObject(map_fields, obj_item->string, field_value_obj);
                obj_item = obj_item->next;
            }
            
            // Добавляем объект полей в mapValue
            cJSON_AddItemToObject(map_value, "mapValue", cJSON_CreateObject());
            cJSON_AddItemToObject(cJSON_GetObjectItem(map_value, "mapValue"), "fields", map_fields);
            
            // Заменяем value_obj на map_value
            cJSON_Delete(value_obj);
            value_obj = map_value;
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

// Public function implementations

esp_err_t firebase_init(void) {
    // Проверяем, установлено ли время
    time_t now = 0;
    time(&now);

    if (now < 1600000000) {
        ESP_LOGE(TAG, "Time not set correctly. Please configure SNTP first.");
        return ESP_FAIL;
    }

    // Генерируем начальный JWT токен
    esp_err_t err = create_jwt_token();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create initial JWT token");
        return err;
    }

    ESP_LOGI(TAG, "Firebase initialized successfully");
    return ESP_OK;
}

// Отправляет данные в Firestore
esp_err_t firebase_send_data(const char *collection, const char *document_id, const char *json_data) {
    // Проверка параметров
    if (!collection || !json_data) {
        return ESP_ERR_INVALID_ARG;
    }

    // Format data for Firestore
    char *firestore_data = format_firestore_data(json_data);
    if (firestore_data == NULL) {
        ESP_LOGE(TAG, "Failed to format data for Firestore");
        return ESP_FAIL;
    }
    
    // Check and refresh token if needed
    if (!is_token_valid()) {
        ESP_LOGI(TAG, "Token expired or about to expire, generating new token");
        if (create_jwt_token() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create new JWT token");
            free(firestore_data);
            return ESP_FAIL;
        }
    }

    // Format URL
    char url[256];
    if (document_id != NULL && strlen(document_id) > 0) {
        // Update/create specific document
        snprintf(url, sizeof(url), "%s/%s/%s", FIREBASE_URL, collection, document_id);
    } else {
        // Create document with auto-generated ID
        snprintf(url, sizeof(url), "%s/%s", FIREBASE_URL, collection);
    }

    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = firebase_http_event_handler,
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = firebase_root_cert,
        .buffer_size = 2048,
        .buffer_size_tx = 4096,
        .timeout_ms = 10000,
        .port = 443,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(firestore_data);
        return ESP_FAIL;
    }

    // Set headers
    char auth_header[4096];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", jwt_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Set request body
    esp_http_client_set_post_field(client, firestore_data, strlen(firestore_data));

    // Perform HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP status code: %d", status_code);
        
        if (status_code < 200 || status_code >= 300) {
            ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
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
