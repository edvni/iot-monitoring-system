// json_helper.c
#include "json_helper.h"
#include "esp_log.h"
#include "cJSON.h"
#include "time_manager.h"
#include <string.h>
#include <math.h>
#include <inttypes.h>

static const char *TAG = "json_helper";

// Extracts MAC address from Firestore JSON document
esp_err_t json_helper_extract_mac_address(const char *json_data, char *mac_address, size_t mac_address_len) {
    if (!json_data || !mac_address || mac_address_len < 18) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Parsing JSON to extract MAC address
    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON data");
        return ESP_FAIL;
    }
    
    // Extracting MAC address from JSON document
    cJSON *fields = cJSON_GetObjectItem(root, "fields");
    if (!fields) {
        ESP_LOGE(TAG, "No 'fields' object in JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // Trying to get MAC address first from "macAddress" field, then from "tag_id"
    cJSON *mac_obj = cJSON_GetObjectItem(fields, "macAddress");
    if (!mac_obj) {
        // If there is no "macAddress" field, try to find "tag_id"
        mac_obj = cJSON_GetObjectItem(fields, "tag_id");
        if (!mac_obj) {
            ESP_LOGE(TAG, "Neither 'macAddress' nor 'tag_id' field found in JSON");
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Using 'tag_id' field for MAC address");
    } else {
        ESP_LOGI(TAG, "Using 'macAddress' field for MAC address");
    }
    
    cJSON *string_value = cJSON_GetObjectItem(mac_obj, "stringValue");
    if (!string_value || !cJSON_IsString(string_value)) {
        ESP_LOGE(TAG, "Invalid MAC address format in JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    strncpy(mac_address, string_value->valuestring, mac_address_len - 1);
    mac_address[mac_address_len - 1] = '\0';
    
    cJSON_Delete(root);
    return ESP_OK;
}

// Formats MAC address, replacing colons with underscores
void json_helper_format_mac_address(const char *mac_address, char *formatted_mac, size_t formatted_mac_len) {
    if (!mac_address || !formatted_mac || formatted_mac_len == 0) {
        return;
    }
    
    strncpy(formatted_mac, mac_address, formatted_mac_len - 1);
    formatted_mac[formatted_mac_len - 1] = '\0';
    
    for (int i = 0; i < strlen(formatted_mac); i++) {
        if (formatted_mac[i] == ':') {
            formatted_mac[i] = '_';
        }
    }
}

// Generates a document ID based on date and MAC address
void json_helper_generate_document_id(const char *time_str, const char *formatted_mac, char *document_id, size_t document_id_len) {
    if (!time_str || !formatted_mac || !document_id || document_id_len == 0) {
        return;
    }
    
    snprintf(document_id, document_id_len, "%s_%s", time_str, formatted_mac);
}

// Creates a new Firestore document structure or updates an existing one
cJSON* json_helper_create_or_update_firestore_document(cJSON* existing_doc, const char* mac_address, uint32_t battery_voltage_mv, int battery_level) {
    cJSON *firestore_doc = existing_doc;
    cJSON *firestore_fields = NULL;
    cJSON *measurements_field = NULL;
    cJSON *array_value = NULL;
    cJSON *values_array = NULL;
    
    if (firestore_doc == NULL) {
        // If the document does not exist, create a new structure
        firestore_doc = cJSON_CreateObject();
        firestore_fields = cJSON_CreateObject();
        
        // Add tag_id (MAC-address) as a field
        cJSON *tag_id_field = cJSON_CreateObject();
        cJSON_AddStringToObject(tag_id_field, "stringValue", mac_address);
        cJSON_AddItemToObject(firestore_fields, "tag_id", tag_id_field);
        
        // Add current date as a field
        char time_str[32];
        if (time_manager_get_formatted_time(time_str, sizeof(time_str)) != ESP_OK) {
            strcpy(time_str, "Time not available");
        }
        
        // Extract only the date (first 10 characters)
        char day[11] = {0}; // YYYY-MM-DD\0
        strncpy(day, time_str, 10);
        day[10] = '\0';
        
        cJSON *day_field = cJSON_CreateObject();
        cJSON_AddStringToObject(day_field, "stringValue", day);
        cJSON_AddItemToObject(firestore_fields, "day", day_field);
        
        // Add battery information
        char battery_voltage_str[10];
        char battery_level_str[5];
        
        snprintf(battery_voltage_str, sizeof(battery_voltage_str), "%" PRIu32, battery_voltage_mv);
        snprintf(battery_level_str, sizeof(battery_level_str), "%d", battery_level);
        
        cJSON *battery_voltage_field = cJSON_CreateObject();
        cJSON_AddStringToObject(battery_voltage_field, "stringValue", battery_voltage_str);
        cJSON_AddItemToObject(firestore_fields, "battery_voltage", battery_voltage_field);
        
        cJSON *battery_level_field = cJSON_CreateObject();
        cJSON_AddStringToObject(battery_level_field, "stringValue", battery_level_str);
        cJSON_AddItemToObject(firestore_fields, "battery_level", battery_level_field);
        
        // Create an array of measurements
        measurements_field = cJSON_CreateObject();
        array_value = cJSON_CreateObject();
        values_array = cJSON_CreateArray();
        
        cJSON_AddItemToObject(array_value, "values", values_array);
        cJSON_AddItemToObject(measurements_field, "arrayValue", array_value);
        cJSON_AddItemToObject(firestore_fields, "measurements", measurements_field);
        
        // Add fields to the root object
        cJSON_AddItemToObject(firestore_doc, "fields", firestore_fields);
    }
    
    return firestore_doc;
}

// Adds a measurement to the Firestore document
esp_err_t json_helper_add_measurement_to_firestore(cJSON* firestore_doc, ruuvi_measurement_t* measurement) {
    if (!firestore_doc || !measurement) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get access to the measurements array
    cJSON *firestore_fields = cJSON_GetObjectItem(firestore_doc, "fields");
    if (!firestore_fields) {
        return ESP_ERR_INVALID_STATE;
    }
    
    cJSON *measurements_field = cJSON_GetObjectItem(firestore_fields, "measurements");
    if (!measurements_field) {
        return ESP_ERR_INVALID_STATE;
    }
    
    cJSON *array_value = cJSON_GetObjectItem(measurements_field, "arrayValue");
    if (!array_value) {
        return ESP_ERR_INVALID_STATE;
    }
    
    cJSON *values_array = cJSON_GetObjectItem(array_value, "values");
    if (!values_array) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get the current time
    char time_str[32];
    if (time_manager_get_formatted_time(time_str, sizeof(time_str)) != ESP_OK) {
        strcpy(time_str, "Time not available");
    }
    
    // Create an object for the measurement
    cJSON *measurement_map_obj = cJSON_CreateObject();
    cJSON *measurement_map_value = cJSON_CreateObject();
    cJSON *measurement_map_fields = cJSON_CreateObject();
    
    // Add fields for temperature, humidity and time
    
    // Temperature
    cJSON *temp_field = cJSON_CreateObject();
    float rounded_temp = roundf(measurement->temperature * 100) / 100;
    char temp_str[10];
    sprintf(temp_str, "%.2f", rounded_temp);
    cJSON_AddStringToObject(temp_field, "stringValue", temp_str);
    cJSON_AddItemToObject(measurement_map_fields, "t", temp_field);
    
    // Humidity
    cJSON *hum_field = cJSON_CreateObject();
    float rounded_hum = roundf(measurement->humidity * 100) / 100;
    char hum_str[10];
    sprintf(hum_str, "%.2f", rounded_hum);
    cJSON_AddStringToObject(hum_field, "stringValue", hum_str);
    cJSON_AddItemToObject(measurement_map_fields, "h", hum_field);
    
    // Timestamp
    cJSON *timestamp_field = cJSON_CreateObject();
    // cJSON_AddStringToObject(timestamp_field, "stringValue", timestamp);
    cJSON_AddStringToObject(timestamp_field, "stringValue", time_str);
    cJSON_AddItemToObject(measurement_map_fields, "ts", timestamp_field);
    
    // Add to the structure
    cJSON_AddItemToObject(measurement_map_value, "fields", measurement_map_fields);
    cJSON_AddItemToObject(measurement_map_obj, "mapValue", measurement_map_value);
    cJSON_AddItemToArray(values_array, measurement_map_obj);
    
    return ESP_OK;
}