# components/cloud_client/CMakeLists.txt
idf_component_register(
    SRCS "cloud_client.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client mbedtls json_helper)