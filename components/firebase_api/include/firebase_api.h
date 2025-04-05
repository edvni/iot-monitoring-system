#ifndef FIREBASE_API_H
#define FIREBASE_API_H

#include "esp_err.h"

/** 
* @brief Initialize Firebase connection
* 
* @note This function must be called after the system time has been set
* 
* @return ESP_OK on success, ESP_FAIL on failure
*/
esp_err_t firebase_init(void);

/**
 * @brief Send RuuviTag sensor data to Firestore
 * 
 * @param temperature Temperature reading in Celsius
 * @param humidity Relative humidity percentage
 * @param tag_id RuuviTag MAC address or identifier
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t firebase_send_ruuvitag_data(const char *tag_id, float temperature, float humidity);

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

#endif /* FIREBASE_API_H */