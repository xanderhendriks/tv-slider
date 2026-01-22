#include "slider.h"

#include "app_mqtt.h"
#include "drv8452.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "hall_sensors.h"
#include "position.h"
#include "slider_state_machine.h"

#define SPEED_MIN_HZ 40000
#define SPEED_MAX_HZ 150000

static const char           *TAG = "slider";
static slider_state_machine  slider_sm;
static QueueHandle_t         slider_event_queue;
static TimerHandle_t         slider_timer;
static hall_sensors_handle_t slider_hall_sensors;
static drv8452_handle_t      slider_drv8452;

static uint8_t sensor_state();
static void    event_task(void *arg);
static void    timer_callback(TimerHandle_t xTimer);

void slider_init(hall_sensors_handle_t hall_sensors, drv8452_handle_t drv8452)
{
    slider_hall_sensors = hall_sensors;
    slider_drv8452      = drv8452;

    slider_state_machine_ctor(&slider_sm);
    slider_state_machine_start(&slider_sm);

    // Create event queue and start the event task
    slider_event_queue = xQueueCreate(16, sizeof(slider_state_machine_EventId));
    configASSERT(slider_event_queue != NULL);
    xTaskCreate(event_task, "slider_event", 4096, NULL, 5, NULL);
}

void slider_motor_enable(bool enable)
{
    ESP_LOGI(TAG, "Slider motor %s", enable ? "enabled" : "disabled");

    // drv8452_sleep(slider_drv8452, !enable);
    ESP_ERROR_CHECK(drv8452_register_write(slider_drv8452, DRV8452_REG_CTRL1,
                                           enable ? DRV8452_CTRL1_EN_OUT_ENABLED : DRV8452_CTRL1_EN_OUT_DISABLED));
}

void slider_speed_set(uint16_t speed)
{
    uint32_t speed_hz = SPEED_MIN_HZ + ((SPEED_MAX_HZ - SPEED_MIN_HZ) * speed) / 100;

    ESP_LOGI(TAG, "Slider speed set to %u", speed_hz);

    drv8452_step_frequency(slider_drv8452, speed_hz);
}

void slider_direction_set(slider_direction_t direction)
{
    ESP_LOGI(TAG, "Slider direction set to %s", direction == SLIDER_DIRECTION_IN ? "IN" : "OUT");

    drv8452_direction(slider_drv8452, direction == SLIDER_DIRECTION_IN);
}

void slider_start_timer(uint32_t interval_ms)
{
    ESP_LOGI(TAG, "Slider timer started for %u ms", interval_ms);

    // Create timer on first call
    if (slider_timer == NULL)
    {
        slider_timer = xTimerCreate("slider_timer", pdMS_TO_TICKS(interval_ms),
                                    pdFALSE,  // don't auto-reload
                                    NULL,     // no timer id needed
                                    timer_callback);
        configASSERT(slider_timer != NULL);
    }
    else
    {
        // If timer already exists, change the period and restart
        xTimerChangePeriod(slider_timer, pdMS_TO_TICKS(interval_ms), 0);
    }

    // Start or restart the timer
    xTimerStart(slider_timer, 0);
}

// Public API to post events to the slider state machine
bool slider_post_event(slider_state_machine_EventId event)
{
    if (slider_event_queue == NULL)
    {
        return false;
    }
    return xQueueSend(slider_event_queue, &event, 0) == pdTRUE;
}

bool slider_is_at_in_stop()
{
    bool at_in_stop = sensor_state() & (1 << HALL_SENSOR_IN_STOP);

    ESP_LOGI(TAG, "Slider is at IN_STOP: %s", at_in_stop ? "true" : "false");

    return at_in_stop;
}

bool slider_is_at_out_stop()
{
    bool at_out_stop = sensor_state() & (1 << HALL_SENSOR_OUT_STOP);

    ESP_LOGI(TAG, "Slider is at OUT_STOP: %s", at_out_stop ? "true" : "false");

    return at_out_stop;
}

void slider_fault_handler()
{
    ESP_LOGE(TAG, "Slider motor fault detected");
    // Additional fault handling code can be added here
}

void slider_status(bool on)
{
    ESP_LOGI(TAG, "Slider status update: %s", on ? "on" : "off");
    // Publish status via MQTT
    mqtt_publish_status(on);
}

void slider_position(int32_t position)
{
    // ESP_LOGI(TAG, "Slider position update: %" PRId32, position);
    // Publish position via MQTT doesn't work here. All these callbacks are working directly from ISR context and should
    // be rewritten. mqtt_publish_position(position);
    position_set(position);
}

static uint8_t sensor_state()
{
    uint8_t state_mask = 0;
    if (slider_hall_sensors != NULL)
    {
        hall_sensors_get_state(slider_hall_sensors, &state_mask);
    }
    return state_mask;
}

// Task: waits for events on the queue and dispatches them to the state machine
static void event_task(void *arg)
{
    (void) arg;
    slider_state_machine_EventId event;
    for (;;)
    {
        if (xQueueReceive(slider_event_queue, &event, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "Processing slider event %s", slider_state_machine_event_id_to_string(event));
            slider_state_machine_dispatch_event(&slider_sm, event);
        }
    }
}

// Timer callback: posts TIMER_EXPIRED event when timer expires
static void timer_callback(TimerHandle_t xTimer)
{
    (void) xTimer;
    slider_post_event(slider_state_machine_EventId_TIMER_EXPIRED);
}