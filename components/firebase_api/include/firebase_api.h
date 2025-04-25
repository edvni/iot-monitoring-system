/**
 * @file firebase_api.h
 * @brief Header for Firebase API functions
 */

#ifndef FIREBASE_API_H
#define FIREBASE_API_H

#include "esp_err.h"
#include <time.h>
#include "cJSON.h"

/**
 * @brief Initialize Firebase API
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t firebase_init(void);

// /**
//  * @brief Send data to Firestore
//  * 
//  * @param collection Firestore collection name
//  * @param document_id Document ID (if NULL, will be auto-generated)
//  * @param json_data JSON data to send
//  * @return esp_err_t ESP_OK on success, error code otherwise
//  */
// esp_err_t firebase_send_data(const char *collection, const char *document_id, const char *json_data);

// /**
//  * @brief Send pre-formatted Firestore data
//  * This function skips the conversion step as data is already in Firestore format
//  * 
//  * @param collection Firestore collection name
//  * @param document_id Document ID (if NULL, will be auto-generated)
//  * @param firestore_data Data already formatted for Firestore
//  * @return esp_err_t ESP_OK on success, error code otherwise
//  */
// esp_err_t firebase_send_firestore_data(const char *collection, const char *document_id, const char *firestore_data);

// /**
//  * @brief Send sensor data with retries using a separate task
//  * 
//  * @param tag_id The sensor tag ID
//  * @param temperature The temperature reading
//  * @param humidity The humidity reading
//  * @param max_retries Maximum number of retry attempts
//  * @return esp_err_t ESP_OK on success, otherwise an error code
//  */
// esp_err_t firebase_send_sensor_data_with_retries(const char *tag_id, float temperature, float humidity, const char *timestamp, int max_retries);

/**
 * @brief Send all measurements to Firestore
 * 
 * @param measurements JSON string with measurements
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t send_final_measurements_to_firebase(const char *measurements);

// /**
//  * @brief Send chunked data to Firestore
//  * 
//  * @param collection Firestore collection name
//  * @param document_id Document ID (if NULL, will be auto-generated)
//  * @param firestore_data Data already formatted for Firestore
//  * @return esp_err_t ESP_OK on success, error code otherwise
//  */
// esp_err_t firebase_send_chunked_data(const char *collection, const char *document_id, const char *firestore_data);

/**
 * @brief Send streamed data to Firestore
 * 
 * @param collection Firestore collection name
 * @param document_id Document ID (if NULL, will be auto-generated)
 * @param firestore_data Data already formatted for Firestore
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t firebase_send_streamed_data(const char *collection, const char *document_id, const char *firestore_data);

#endif /* FIREBASE_API_H */