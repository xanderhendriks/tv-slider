#include "position.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE s_position_lock = portMUX_INITIALIZER_UNLOCKED;
static int32_t      s_position      = 0;

void position_set(int32_t position)
{
    portENTER_CRITICAL(&s_position_lock);
    s_position = position;
    portEXIT_CRITICAL(&s_position_lock);
}

int32_t position_get(void)
{
    int32_t value;
    portENTER_CRITICAL(&s_position_lock);
    value = s_position;
    portEXIT_CRITICAL(&s_position_lock);

    // Scale raw encoder count [0, POSITION_MAX_ENCODER_COUNT] to [0, 100]
    if (value <= 0)
    {
        return 0;
    }
    if (value >= POSITION_MAX_ENCODER_COUNT)
    {
        return 100;
    }
    return (value * 100) / POSITION_MAX_ENCODER_COUNT;
}
