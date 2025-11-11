idf_component_register(SRCS "main.c" "system.c"
                       REQUIRES drv8452
                       PRIV_REQUIRES spi_flash esp_driver_ledc esp_driver_gpio esp_driver_spi
                       INCLUDE_DIRS "")
