#include "config.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG               = "config";
static const char *NVS_NAMESPACE     = "tv_slider";
static const char *NVS_KEY_MQTT_SRV  = "mqtt_server";
static const char *NVS_KEY_MQTT_TOP  = "mqtt_topic";
static const char *NVS_KEY_MQTT_PORT = "mqtt_port";
static const char *NVS_KEY_INV_IN    = "invert_inputs";

static config_data_t s_current_config;

static void config_set_defaults(config_data_t *config)
{
    if (config == NULL)
    {
        return;
    }

    strncpy(config->mqtt_server, "192.168.0.253", CONFIG_MQTT_SERVER_MAX_LEN - 1);
    config->mqtt_server[CONFIG_MQTT_SERVER_MAX_LEN - 1] = '\0';

    strncpy(config->mqtt_topic, "tv_slider", CONFIG_MQTT_TOPIC_MAX_LEN - 1);
    config->mqtt_topic[CONFIG_MQTT_TOPIC_MAX_LEN - 1] = '\0';

    config->mqtt_port     = 1883;
    config->invert_inputs = false;
}

esp_err_t config_init(void)
{
    esp_err_t err;

    // Set defaults first
    config_set_defaults(&s_current_config);

    // Try to load from NVS
    nvs_handle_t handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        // No configuration stored yet, use defaults
        ESP_LOGI(TAG, "No configuration found in NVS, using defaults");
        return ESP_OK;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for reading (%s)", esp_err_to_name(err));
        return err;
    }

    // Load MQTT server
    size_t len = CONFIG_MQTT_SERVER_MAX_LEN;
    err        = nvs_get_str(handle, NVS_KEY_MQTT_SRV, s_current_config.mqtt_server, &len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Failed to read MQTT server from NVS (%s)", esp_err_to_name(err));
    }

    // Load MQTT topic
    len = CONFIG_MQTT_TOPIC_MAX_LEN;
    err = nvs_get_str(handle, NVS_KEY_MQTT_TOP, s_current_config.mqtt_topic, &len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Failed to read MQTT topic from NVS (%s)", esp_err_to_name(err));
    }

    // Load MQTT port
    uint16_t port = 0;
    err           = nvs_get_u16(handle, NVS_KEY_MQTT_PORT, &port);
    if (err == ESP_OK)
    {
        s_current_config.mqtt_port = port;
    }
    else if (err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Failed to read mqtt_port from NVS (%s)", esp_err_to_name(err));
    }

    // Load invert inputs
    uint8_t invert = 0;
    err            = nvs_get_u8(handle, NVS_KEY_INV_IN, &invert);
    if (err == ESP_OK)
    {
        s_current_config.invert_inputs = (invert != 0);
    }
    else if (err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Failed to read invert_inputs from NVS (%s)", esp_err_to_name(err));
    }

    nvs_close(handle);

    ESP_LOGI(TAG, "Configuration loaded - MQTT server: %s, port: %u, topic: %s, invert_inputs: %d",
             s_current_config.mqtt_server, (unsigned) s_current_config.mqtt_port, s_current_config.mqtt_topic,
             s_current_config.invert_inputs);

    return ESP_OK;
}

esp_err_t config_load(config_data_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(config, &s_current_config, sizeof(config_data_t));
    return ESP_OK;
}

esp_err_t config_save(const config_data_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t    err;
    nvs_handle_t handle;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for writing (%s)", esp_err_to_name(err));
        return err;
    }

    // Save MQTT server
    err = nvs_set_str(handle, NVS_KEY_MQTT_SRV, config->mqtt_server);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write MQTT server to NVS (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Save MQTT topic
    err = nvs_set_str(handle, NVS_KEY_MQTT_TOP, config->mqtt_topic);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write MQTT topic to NVS (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Save MQTT port
    err = nvs_set_u16(handle, NVS_KEY_MQTT_PORT, config->mqtt_port);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write mqtt_port to NVS (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Save invert inputs
    uint8_t invert = config->invert_inputs ? 1 : 0;
    err            = nvs_set_u8(handle, NVS_KEY_INV_IN, invert);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write invert_inputs to NVS (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Commit changes
    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS changes (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    nvs_close(handle);

    // Update current config
    memcpy(&s_current_config, config, sizeof(config_data_t));

    ESP_LOGI(TAG, "Configuration saved - MQTT server: %s, port: %u, topic: %s, invert_inputs: %d",
             s_current_config.mqtt_server, (unsigned) s_current_config.mqtt_port, s_current_config.mqtt_topic,
             s_current_config.invert_inputs);

    return ESP_OK;
}

esp_err_t config_get(config_data_t *config)
{
    return config_load(config);
}

esp_err_t config_reset(void)
{
    config_data_t defaults;
    config_set_defaults(&defaults);
    return config_save(&defaults);
}
