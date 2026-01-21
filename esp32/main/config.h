#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CONFIG_MQTT_SERVER_MAX_LEN 64
#define CONFIG_MQTT_TOPIC_MAX_LEN  128

    typedef struct
    {
        char     mqtt_server[CONFIG_MQTT_SERVER_MAX_LEN];
        char     mqtt_topic[CONFIG_MQTT_TOPIC_MAX_LEN];
        uint16_t mqtt_port;
        bool     invert_inputs;
    } config_data_t;

    /**
     * @brief Initialize the configuration module and load settings from NVS
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t config_init(void);

    /**
     * @brief Load configuration from NVS
     *
     * @param config Pointer to config_data_t structure to fill
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t config_load(config_data_t *config);

    /**
     * @brief Save configuration to NVS
     *
     * @param config Pointer to config_data_t structure to save
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t config_save(const config_data_t *config);

    /**
     * @brief Get the currently loaded configuration
     *
     * @param config Pointer to config_data_t structure to fill
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t config_get(config_data_t *config);

    /**
     * @brief Reset configuration to defaults
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t config_reset(void);

#ifdef __cplusplus
}
#endif
