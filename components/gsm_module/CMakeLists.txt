idf_component_register(
    SRCS "gsm_module.c"	
    INCLUDE_DIRS "include"
    REQUIRES driver esp_system freertos esp_common esp_modem esp_netif esp_event lwip
)
