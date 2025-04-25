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
//  * @brief Send final measurements to Firebase
//  * 
//  * Uses the same logic as process_and_send_sensor_data,
//  * but takes data directly instead of reading from a file.
//  * 
//  * @param measurements String JSON with measurement data
//  * @return esp_err_t ESP_OK on success
//  */
// esp_err_t send_final_measurements_to_firebase(const char *measurements);

/**
 * @brief Send all sensor data from storage to Firebase
 * 
 * This function implements the mechanism for sending data for multiple sensors:
 * 1. Gets the list of all sensor files from storage_get_sensor_files()
 * 2. Processes each file separately, extracting the MAC address from the data
 * 3. Generates a unique document_id for each sensor, including date and MAC
 * 4. Sends data as separate documents in Firestore
 * 5. Deletes files after successful sending
 * 
 * Note: Before calling this function, Firebase must be initialized via firebase_init()
 * 
 * @return esp_err_t 
 *         - ESP_OK: all files sent successfully
 *         - ESP_ERR_INVALID_STATE: some files sent successfully
 *         - ESP_FAIL: none of the files sent
 *         - ESP_ERR_NOT_FOUND: files not found
 */
esp_err_t send_all_sensor_measurements_to_firebase(void);

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