idf_component_register(SRCS "app_mqtt.c" "webserver.c" "provisioning.c" "console.c" "main.c" "system.c"
                       REQUIRES drv8452 console shaft_encoder hall_sensors led
                       PRIV_REQUIRES esp_event esp_http_server esp_netif lwip mqtt esp_wifi nvs_flash wifi_provisioning spi_flash esp_driver_ledc esp_driver_gpio esp_driver_spi esp_driver_uart app_update
                       INCLUDE_DIRS ""
                       EMBED_FILES "assets/index.html"
                                   "assets/favicon.ico")
