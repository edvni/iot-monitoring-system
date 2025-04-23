// json_helper.c
#include "json_helper.h"
#include "esp_log.h"
#include "cJSON.h"
#include "time_manager.h"
#include <string.h>
#include <math.h>


// // Create a JSON object from a measurement
// cJSON* json_helper_create_measurement_object(ruuvi_measurement_t *measurement) {
//     cJSON *measurement_obj = cJSON_CreateObject();
//     if (measurement_obj == NULL) {
//         ESP_LOGE(TAG, "Failed to create measurement object");
//         return NULL;
//     }

//     // Buffer for time
//     char time_str[32];
//     if (time_manager_get_formatted_time(time_str, sizeof(time_str)) != ESP_OK) {
//         strcpy(time_str, "Time not available");
//     }

//     // Add time stamp
//     cJSON_AddStringToObject(measurement_obj, "time", time_str);

//     // Existing fields
//     char temp_str[10];
//     char hum_str[10];
//     //char mac_str[18];
//     sprintf(temp_str, "%.2f", measurement->temperature);
//     snprintf(hum_str, sizeof(hum_str), "%.2f", measurement->humidity);
//     //snprintf(mac_str, sizeof(mac_str), "%s", measurement->mac_address);
//     cJSON_AddStringToObject(measurement_obj, "t", temp_str);
//     cJSON_AddStringToObject(measurement_obj, "h", hum_str);
//     // cJSON_AddStringToObject(measurement_obj, "mac", mac_str);

//     return measurement_obj;
// }


// // Transforming one measurement to JSON string
// char* json_helper_measurement_to_string(ruuvi_measurement_t *measurement) {
//     cJSON *obj = json_helper_create_measurement_object(measurement);
//     if (obj == NULL) {
//         return NULL;
//     }

//     char *string = cJSON_PrintUnformatted(obj);
//     cJSON_Delete(obj);

//     return string;
// }


// Creates a new Firestore document structure or updates an existing one
cJSON* json_helper_create_or_update_firestore_document(cJSON* existing_doc, const char* mac_address) {
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
    
    // Extract only time (last 8 characters, if available)
    // char timestamp[9] = {0};
    // if (strlen(time_str) >= 19) {
    //     strncpy(timestamp, time_str + 11, 8);
    //     timestamp[8] = '\0';
    // } else {
    //     strncpy(timestamp, time_str, 8);
    //     timestamp[8] = '\0';
    // }
    
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