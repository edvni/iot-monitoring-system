idf_component_register(
   SRCS "discord_api.cpp"
   INCLUDE_DIRS "include"
   REQUIRES esp_http_client json esp_wifi esp_netif lwip esp-tls storage
)