#include "slider.h"

#include "drv8452.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "hall_sensors.h"
#include "slider_state_machine.h"

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
    // Implementation to enable or disable the slider motor
    if (enable)
    {
        // Code to enable the motor
    }
    else
    {
        // Code to disable the motor
    }
}

void slider_speed_set(uint16_t speed)
{
    ESP_LOGI(TAG, "Slider speed set to %u", speed);
    // Implementation to set the speed of the slider motor
    // Code to adjust motor speed based on the provided value
}

void slider_direction_set(slider_direction_t direction)
{
    ESP_LOGI(TAG, "Slider direction set to %s", direction == SLIDER_DIRECTION_IN ? "IN" : "OUT");
    // Implementation to set the direction of the slider motor
    if (direction == SLIDER_DIRECTION_IN)
    {
        // Code to set motor direction to IN
    }
    else
    {
        // Code to set motor direction to OUT
    }
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
            ESP_LOGI(TAG, "Processing slider event %d", event);
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