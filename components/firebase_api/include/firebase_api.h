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
 * @brief Инициализация Firebase API
 * 
 * @return esp_err_t ESP_OK в случае успеха, иначе код ошибки
 */
esp_err_t firebase_init(void);

/**
 * @brief Отправляет данные в Firestore
 * 
 * @param collection Имя коллекции в Firestore
 * @param document_id ID документа (если NULL, будет сгенерирован автоматически)
 * @param json_data JSON данные для отправки
 * @return esp_err_t ESP_OK в случае успеха, иначе код ошибки
 */
esp_err_t firebase_send_data(const char *collection, const char *document_id, const char *json_data);

/**
 * @brief Send sensor data with retries using a separate task
 * 
 * @param tag_id The sensor tag ID
 * @param temperature The temperature reading
 * @param humidity The humidity reading
 * @param max_retries Maximum number of retry attempts
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
// esp_err_t firebase_send_sensor_data_with_retries(const char *tag_id, float temperature, float humidity, const char *timestamp, int max_retries);

/**
 * @brief Отправляет все измерения в Firestore
 * 
 * @param measurements Строка JSON с измерениями
 * @return esp_err_t ESP_OK в случае успеха, иначе код ошибки
 */
esp_err_t send_final_measurements_to_firebase(const char *measurements);

#endif /* FIREBASE_API_H */