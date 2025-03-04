// json_helper.c
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "json_helper.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "JSON_HELPER";

// Create a JSON object from a measurement
cJSON* json_helper_create_measurement_object(ruuvi_measurement_t *measurement) {
    cJSON *measurement_obj = cJSON_CreateObject();
    if (measurement_obj == NULL) {
        ESP_LOGE(TAG, "Failed to create measurement object");
        return NULL;
    }

    // Buffers for rounded values
    char temp_str[10];
    char hum_str[10];

    // Formatting numbers as strings with two decimal places
    snprintf(temp_str, sizeof(temp_str), "%.f", measurement->temperature);
    snprintf(hum_str, sizeof(hum_str), "%.f", measurement->humidity);

    // Adding to JSON as strings
    cJSON_AddStringToObject(measurement_obj, "t", temp_str);
    cJSON_AddStringToObject(measurement_obj, "h", hum_str);

    //cJSON_AddStringToObject(measurement_obj, "mac", measurement->mac_address);
    //cJSON_AddNumberToObject(measurement_obj, "temperature", measurement->temperature);
    //cJSON_AddNumberToObject(measurement_obj, "humidity", measurement->humidity);
    //cJSON_AddNumberToObject(measurement_obj, "timestamp", measurement->timestamp);

    return measurement_obj;
}

// Transforming one measurement to JSON string
char* json_helper_measurement_to_string(ruuvi_measurement_t *measurement) {
    cJSON *obj = json_helper_create_measurement_object(measurement);
    if (obj == NULL) {
        return NULL;
    }

    char *string = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);

    return string;
}