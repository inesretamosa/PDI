#include "riscv_types.h"
#include "riscv_uart.h"
#include "gpio_drv.h"

#include "dispatch.h"
#include "clinc.h"
#include "riscv_monotonic_clock.h"

#include "log.h"

#define TICKS_PER_MS 10000ULL
#define ONE_SECOND_MS 1000ULL
#define BLINK_MS 500ULL

int main(void)
{
    uint64_t press_ticks = 0;
    uint64_t release_ticks = 0;
    uint64_t last_blink_ticks = 0;
    uint64_t elapsed_ms = 0;

    uint8_t button0_prev = 0;
    uint8_t blinking = 0;
    uint8_t leds_on = 0;

    gpio_set_direction(LED_0_MASK | LED_1_MASK | LED_2_MASK | LED_3_MASK);

    while (1)
    {
        uint32_t buttons = gpio_read();
        uint64_t now_ticks = get_ticks_from_reset();

        if ((buttons & PBT_0_MASK) && !button0_prev)
        {
            press_ticks = now_ticks;
            button0_prev = 1;
        }
        else if (!(buttons & PBT_0_MASK) && button0_prev)
        {
            release_ticks = now_ticks;
            elapsed_ms =
                (release_ticks - press_ticks) / TICKS_PER_MS;

            printf("Tiempo pulsado: %llu ms\n", elapsed_ms);

            if (elapsed_ms > ONE_SECOND_MS)
            {
                blinking = 1;
                last_blink_ticks = now_ticks;
            }

            button0_prev = 0;
        }
        else if (blinking)
        {
            if ((now_ticks - last_blink_ticks) >=
                (BLINK_MS * TICKS_PER_MS))
            {
                last_blink_ticks = now_ticks;

                if (leds_on)
                {
                    gpio_write(0);
                    leds_on = 0;
                }
                else
                {
                    gpio_write(LED_0_MASK | LED_1_MASK | LED_2_MASK | LED_3_MASK);
                    leds_on = 1;
                }
            }

            if (buttons & PBT_1_MASK)
            {
                blinking = 0;
                leds_on = 0;
                gpio_write(0);
            }
        }
    }
}
