#include "app_mqtt.h"

#include <inttypes.h>
#include <string.h>
#include <strings.h>

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt_client";

#define MQTT_SERVER "192.168.0.253"
#define MQTT_PORT   1883

#define MQTT_TOPIC_SWITCH   "tv-slider/switch"
#define MQTT_TOPIC_STATE    "tv-slider/state"
#define MQTT_TOPIC_POSITION "tv-slider/position"

static esp_mqtt_client_handle_t s_client;
static mqtt_switch_cb_t         s_switch_cb;
static void                    *s_switch_ctx;
static bool                     s_started;

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
            esp_mqtt_client_subscribe(event->client, MQTT_TOPIC_SWITCH, 1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected");
            break;
        case MQTT_EVENT_DATA:
        {
            if (event->topic_len == (int) strlen(MQTT_TOPIC_SWITCH) &&
                strncmp(event->topic, MQTT_TOPIC_SWITCH, event->topic_len) == 0)
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

void mqtt_client_start(void)
{
    if (s_started)
    {
        return;
    }

    char uri[64];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", MQTT_SERVER, MQTT_PORT);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
    };

    s_client = esp_mqtt_client_init(&cfg);
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

void mqtt_publish_state(bool on)
{
    if (!s_client)
    {
        return;
    }

    const char *payload = on ? "on" : "off";
    esp_mqtt_client_publish(s_client, MQTT_TOPIC_STATE, payload, 0, 1, 1);
}

void mqtt_publish_position(int32_t position)
{
    if (!s_client)
    {
        return;
    }

    char payload[16];
    snprintf(payload, sizeof(payload), "%" PRId32, position);
    esp_mqtt_client_publish(s_client, MQTT_TOPIC_POSITION, payload, 0, 1, 1);
}
