idf_component_register(
    SRCS "camera_stream.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp32-camera
        esp_wifi
        esp_http_server
        nvs_flash
        esp_netif
)
