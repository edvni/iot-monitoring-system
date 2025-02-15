// json_helper.h
#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include "esp_err.h"
#include "sensors.h"
#include "cJSON.h"

// Transform one measurement to JSON string
char* json_helper_measurement_to_string(ruuvi_measurement_t *measurement);

// Create a JSON object from a measurement
cJSON* json_helper_create_measurement_object(ruuvi_measurement_t *measurement);

#endif // JSON_HELPER_H