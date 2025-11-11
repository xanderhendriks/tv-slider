idf_component_register(SRCS "console.c" "main.c" "system.c"
                       REQUIRES drv8452 console
                       PRIV_REQUIRES spi_flash esp_driver_ledc esp_driver_gpio esp_driver_spi esp_driver_uart
                       INCLUDE_DIRS "")
