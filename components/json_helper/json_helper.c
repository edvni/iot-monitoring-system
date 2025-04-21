// json_helper.c
#include "json_helper.h"
#include "esp_log.h"
#include "cJSON.h"
#include "time_manager.h"
#include <string.h>
#include <math.h>

static const char *TAG = "JSON_HELPER";

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


// // Create a Firestore-formatted measurement object
// cJSON* json_helper_create_firestore_measurement(ruuvi_measurement_t *measurement) {
//     cJSON *measurement_obj = cJSON_CreateObject();
//     if (measurement_obj == NULL) {
//         ESP_LOGE(TAG, "Failed to create Firestore measurement object");
//         return NULL;
//     }

//     // Create fields object for Firestore format
//     cJSON *fields = cJSON_CreateObject();
//     if (fields == NULL) {
//         ESP_LOGE(TAG, "Failed to create fields object");
//         cJSON_Delete(measurement_obj);
//         return NULL;
//     }

//     // Buffer for time
//     char time_str[32];
//     if (time_manager_get_formatted_time(time_str, sizeof(time_str)) != ESP_OK) {
//         strcpy(time_str, "Time not available");
//     }

//     // Create timestamp field
//     cJSON *time_field = cJSON_CreateObject();
//     cJSON_AddStringToObject(time_field, "stringValue", time_str);
//     cJSON_AddItemToObject(fields, "time", time_field);

//     // Create temperature field
//     cJSON *temp_field = cJSON_CreateObject();
//     float rounded_temp = roundf(measurement->temperature * 100) / 100;
//     char temp_str[10];
//     sprintf(temp_str, "%.2f", rounded_temp);
//     cJSON_AddStringToObject(temp_field, "stringValue", temp_str);
//     cJSON_AddItemToObject(fields, "temperature", temp_field);

//     // Create humidity field
//     cJSON *hum_field = cJSON_CreateObject();
//     float rounded_hum = roundf(measurement->humidity * 100) / 100;
//     char hum_str[10];
//     sprintf(hum_str, "%.2f", rounded_hum);
//     cJSON_AddStringToObject(hum_field, "stringValue", hum_str);
//     cJSON_AddItemToObject(fields, "humidity", hum_field);

//     // Create MAC address field
//     cJSON *mac_field = cJSON_CreateObject();
//     cJSON_AddStringToObject(mac_field, "stringValue", measurement->mac_address);
//     cJSON_AddItemToObject(fields, "mac", mac_field);

//     // Add fields to root object
//     cJSON_AddItemToObject(measurement_obj, "fields", fields);

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

// // Transform one measurement to Firestore format JSON string
// char* json_helper_measurement_to_firestore_string(ruuvi_measurement_t *measurement) {
//     cJSON *obj = json_helper_create_firestore_measurement(measurement);
//     if (obj == NULL) {
//         return NULL;
//     }

//     char *string = cJSON_PrintUnformatted(obj);
//     cJSON_Delete(obj);

//     return string;
// }