cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    "$ENV{IDF_PATH}/components/esp-protocols/components"
    "$ENV{IDF_PATH}/components/esp_netif"
    "$ENV{IDF_PATH}/components/lwip"
)
set(PARTITION_TABLE "partitions.csv")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sensor_project)






