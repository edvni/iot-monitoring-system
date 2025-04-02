idf_component_register(
    SRCS "main.c"
    REQUIRES 
        console 
        nvs_flash 
        esp_netif 
        gsm_modem 
        driver
        discord_api
        power_management
        sensors
        storage
        time_manager
)