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
 * @brief Send mock sensor data to Firestore for testing connection
 * 
 * @param tag_id A mock identifier for the sensor
 * @param num_samples Number of mock readings to send
 * @return esp_err_t ESP_OK if all data was sent successfully, otherwise ESP_FAIL
 */
esp_err_t firebase_send_mock_data(const char *tag_id, int num_samples);

/**
 * @brief Start a task to send mock data to Firebase
 * 
 * @param tag_id A mock identifier for the sensor
 * @param num_samples Number of mock readings to send
 * @return esp_err_t ESP_OK if task started successfully, ESP_FAIL otherwise
 */
esp_err_t firebase_start_mock_data_task(const char *tag_id, int num_samples);

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