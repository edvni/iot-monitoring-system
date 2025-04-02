idf_component_register(
    SRCS "time_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES 
    driver 
    gsm_modem
    esp_timer
    esp_netif
)