idf_component_register(
   SRCS "firebase_api.c"
   INCLUDE_DIRS "include"
   REQUIRES mbedtls esp_http_client json lwip esp_wifi esp_netif lwip esp-tls storage
)