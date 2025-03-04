cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    "$ENV{IDF_PATH}/components/esp-protocols/components"
    "$ENV{IDF_PATH}/components/esp_netif"
    "$ENV{IDF_PATH}/components/lwip"
    "$ENV{IDF_PATH}/components/esp-tls"
    "${CMAKE_CURRENT_SOURCE_DIR}/components"  
)

set(PARTITION_TABLE "partitions.csv")
set(COMPONENTS main esp_hw_support esp_system esp_timer driver esp_wifi esp_netif nvs_flash esp_pm bt)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sensor_project)