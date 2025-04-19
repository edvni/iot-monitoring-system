// json_helper.h
#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include "esp_err.h"
#include "sensors.h"
#include "cJSON.h"


/**
 * @brief Transform one measurement to JSON string
 * 
 * @param measurement Measurement to transform
 * @return char* JSON string
 */
char* json_helper_measurement_to_string(ruuvi_measurement_t *measurement);


/**
 * @brief Create a JSON object from a measurement
 * 
 * @param measurement Measurement to transform
 * @return cJSON* JSON object
 */
cJSON* json_helper_create_measurement_object(ruuvi_measurement_t *measurement);

/**
 * @brief Create a Firestore-formatted measurement object
 * 
 * @param measurement Measurement to transform
 * @return cJSON* Firestore-formatted JSON object
 */
cJSON* json_helper_create_firestore_measurement(ruuvi_measurement_t *measurement);

/**
 * @brief Transform one measurement to Firestore format JSON string
 * 
 * @param measurement Measurement to transform
 * @return char* Firestore-formatted JSON string
 */
char* json_helper_measurement_to_firestore_string(ruuvi_measurement_t *measurement);

#endif // JSON_HELPER_H