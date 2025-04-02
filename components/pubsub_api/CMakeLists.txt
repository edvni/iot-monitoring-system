idf_component_register(
   SRCS "pubsub_api.c"
   INCLUDE_DIRS "include"
   REQUIRES esp_http_client json lwip esp_wifi esp_netif lwip esp-tls storage
)