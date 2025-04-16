/**
 * @file firebase_tasks.h
 * @brief Header for Firebase Firestore data sending through separate tasks
 */

#ifndef FIREBASE_TASKS_H
#define FIREBASE_TASKS_H

#include "esp_err.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send Firebase data safely using a separate task with a large stack
 * 
 * @param collection The Firestore collection name
 * @param document_id The document ID (NULL or empty for auto-generated ID)
 * @param json_data The JSON data to send
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t firebase_send_data_safe(const char *collection, const char *document_id, const char *json_data);

#ifdef __cplusplus
}
#endif

#endif /* FIREBASE_TASKS_H */