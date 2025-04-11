/**
 * @file firebase_api.h
 * @brief Header for Firebase API functions
 */

#ifndef FIREBASE_API_H
#define FIREBASE_API_H

#include "esp_err.h"
#include <time.h>

/** 
* @brief Initialize Firebase connection
* 
* @note This function must be called after the system time has been set
* 
* @return ESP_OK on success, ESP_FAIL on failure
*/
esp_err_t firebase_init(void);

/**
 * @brief Send custom data to Firestore
 * 
 * @param collection Firestore collection name
 * @param document_id Document ID (use NULL for auto-generated ID)
 * @param json_data JSON string with data to send
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t firebase_send_data(const char *collection, const char *document_id, const char *json_data);

/**
 * @brief Send Firebase data safely using a separate task with a large stack
 * 
 * @param collection The Firestore collection name
 * @param document_id The document ID (NULL or empty for auto-generated ID)
 * @param json_data The JSON data to send
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t firebase_send_data_safe(const char *collection, const char *document_id, const char *json_data);

/**
 * @brief Send sensor data with retries using a separate task
 * 
 * @param tag_id The sensor tag ID
 * @param temperature The temperature reading
 * @param humidity The humidity reading
 * @param max_retries Maximum number of retry attempts
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t firebase_send_sensor_data_with_retries(const char *tag_id, float temperature, float humidity, const char *timestamp, int max_retries);


#endif /* FIREBASE_API_H */