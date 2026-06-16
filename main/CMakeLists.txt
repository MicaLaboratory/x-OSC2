idf_component_register(SRCS "main.c"
                    REQUIRES nvs_flash esp_driver_gpio esp_wifi esp_http_server
                    INCLUDE_DIRS ".")
