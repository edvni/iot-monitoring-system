// json_helper.h
#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include "esp_err.h"
#include "sensors.h"
#include "cJSON.h"


// /**
//  * @brief Transform one measurement to JSON string
//  * 
//  * @param measurement Measurement to transform
//  * @return char* JSON string
//  */
// char* json_helper_measurement_to_string(ruuvi_measurement_t *measurement);


// /**
//  * @brief Create a JSON object from a measurement
//  * 
//  * @param measurement Measurement to transform
//  * @return cJSON* JSON object
//  */
// cJSON* json_helper_create_measurement_object(ruuvi_measurement_t *measurement);


/**
 * @brief Create a new Firestore document structure or update an existing one
 * 
 * @param existing_doc Existing document or NULL
 * @param mac_address MAC address of the tag/device
 * @return cJSON* Firestore document structure
 */
cJSON* json_helper_create_or_update_firestore_document(cJSON* existing_doc, const char* mac_address);

/**
 * @brief Add a measurement to the Firestore document
 * 
 * @param firestore_doc Firestore document to add the measurement to
 * @param measurement Measurement data
 * @return esp_err_t ESP_OK on success
 */
esp_err_t json_helper_add_measurement_to_firestore(cJSON* firestore_doc, ruuvi_measurement_t* measurement);


#endif // JSON_HELPER_H