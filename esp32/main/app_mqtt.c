#include "app_mqtt.h"

#include <inttypes.h>
#include <string.h>
#include <strings.h>

#include "config.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt_client";

static char s_topic_switch[CONFIG_MQTT_TOPIC_MAX_LEN + 16];
static char s_topic_state[CONFIG_MQTT_TOPIC_MAX_LEN + 16];
static char s_topic_position[CONFIG_MQTT_TOPIC_MAX_LEN + 16];

static esp_mqtt_client_handle_t s_client;
static mqtt_switch_cb_t         s_switch_cb;
static void                    *s_switch_ctx;
static bool                     s_started;
static config_data_t            s_cfg_cached;

static bool parse_switch_payload(const char *payload)
{
    if (!payload || payload[0] == '\0')
    {
        return false;
    }

    if (strcasecmp(payload, "1") == 0 || strcasecmp(payload, "on") == 0 || strcasecmp(payload, "true") == 0)
    {
        return true;
    }

    return false;
}

static void handle_switch_message(const char *data, int data_len)
{
    if (!s_switch_cb)
    {
        return;
    }

    char payload[32];
    int  copy_len = data_len < (int) (sizeof(payload) - 1) ? data_len : (int) (sizeof(payload) - 1);
    memcpy(payload, data, copy_len);
    payload[copy_len] = '\0';

    bool on = parse_switch_payload(payload);
    s_switch_cb(on, s_switch_ctx);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void) handler_args;
    (void) base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch (event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected");
            esp_mqtt_client_subscribe(event->client, s_topic_switch, 1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected");
            break;
        case MQTT_EVENT_DATA:
        {
            ESP_LOGI(TAG, "Data received: data=%.*s", event->data_len, event->data);
            if (event->topic_len == (int) strlen(s_topic_switch) &&
                strncmp(event->topic, s_topic_switch, event->topic_len) == 0)
            {
                handle_switch_message(event->data, event->data_len);
            }
            break;
        }
        default:
            break;
    }
}

void mqtt_client_init(mqtt_switch_cb_t cb, void *user_ctx)
{
    s_switch_cb  = cb;
    s_switch_ctx = user_ctx;
}

static void mqtt_client_stop_if_running(void)
{
    if (s_client)
    {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    s_started = false;
}

static void mqtt_client_build_topics(const config_data_t *cfg)
{
    snprintf(s_topic_switch, sizeof(s_topic_switch), "%s/switch", cfg->mqtt_topic);
    snprintf(s_topic_state, sizeof(s_topic_state), "%s/state", cfg->mqtt_topic);
    snprintf(s_topic_position, sizeof(s_topic_position), "%s/position", cfg->mqtt_topic);
}

static void mqtt_client_start_with_cfg(const config_data_t *cfg)
{
    mqtt_client_stop_if_running();

    s_cfg_cached = *cfg;
    mqtt_client_build_topics(&s_cfg_cached);

    char uri[96];
    snprintf(uri, sizeof(uri), "mqtt://%s:%u", s_cfg_cached.mqtt_server, (unsigned) s_cfg_cached.mqtt_port);

    esp_mqtt_client_config_t cfg_cli = {
        .broker.address.uri = uri,
    };

    s_client = esp_mqtt_client_init(&cfg_cli);
    if (!s_client)
    {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    if (esp_mqtt_client_start(s_client) == ESP_OK)
    {
        s_started = true;
    }
}

void mqtt_client_start(void)
{
    if (s_started)
    {
        return;
    }

    config_data_t cfg_data;
    if (config_get(&cfg_data) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load MQTT config");
        return;
    }

    mqtt_client_start_with_cfg(&cfg_data);
}

void mqtt_client_apply_config(const config_data_t *cfg)
{
    if (!cfg)
    {
        return;
    }
    mqtt_client_start_with_cfg(cfg);
}

void mqtt_publish_state(bool on)
{
    if (!s_client)
    {
        return;
    }

    const char *payload = on ? "on" : "off";
    esp_mqtt_client_publish(s_client, s_topic_state, payload, 0, 1, 1);
}

void mqtt_publish_position(int32_t position)
{
    if (!s_client)
    {
        return;
    }

    char payload[16];
    snprintf(payload, sizeof(payload), "%" PRId32, position);
    esp_mqtt_client_publish(s_client, s_topic_position, payload, 0, 1, 1);
}

void mqtt_publish_status(bool on)
{
    if (!s_client)
    {
        return;
    }

    const char *payload = on ? "on" : "off";
    esp_mqtt_client_publish(s_client, s_topic_state, payload, 0, 1, 1);
}
