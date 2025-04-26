// json_helper.h
#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include "esp_err.h"
#include "sensors.h"
#include "cJSON.h"


/**
 * @brief Extracts MAC address from Firestore JSON document
 * 
 * @param json_data JSON document as a string
 * @param mac_address Buffer to save MAC address
 * @param mac_address_len Buffer size
 * @return esp_err_t ESP_OK on success
 */
esp_err_t json_helper_extract_mac_address(const char *json_data, char *mac_address, size_t mac_address_len);

/**
 * @brief Formats MAC address, replacing colons with underscores
 * 
 * @param mac_address Original MAC address
 * @param formatted_mac Buffer to save formatted MAC address
 * @param formatted_mac_len Buffer size
 */
void json_helper_format_mac_address(const char *mac_address, char *formatted_mac, size_t formatted_mac_len);

/**
 * @brief Generates a document ID based on date and MAC address
 * 
 * @param time_str Date string
 * @param formatted_mac Formatted MAC address
 * @param document_id Buffer to save document ID
 * @param document_id_len Buffer size
 */
void json_helper_generate_document_id(const char *time_str, const char *formatted_mac, char *document_id, size_t document_id_len);

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