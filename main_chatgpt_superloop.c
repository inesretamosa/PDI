#include "riscv_types.h"
#include "riscv_uart.h"
#include "gpio_drv.h"

#include "dispatch.h"
#include "clinc.h"
#include "riscv_monotonic_clock.h"

#include "log.h"

#define ALL_LEDS     (LED_0_MASK | LED_1_MASK | LED_2_MASK | LED_3_MASK)

int main(void) {
    uint64_t t_start = 0, t_last_blink = 0;
    uint8_t pbt0_last = 0, blink_en = 0, leds_on = 0;
    uint32_t current_gpio;

    /* Configuración: Bits 16-19 como salida */
    gpio_set_direction(ALL_LEDS);

    while (1) {
        current_gpio = gpio_read();
        uint64_t now = get_ticks_from_reset();
        uint8_t pbt0_now = (current_gpio & PBT_0_MASK) ? 1 : 0;

        /* Detección de flanco de bajada (Pulsar) */
        if (pbt0_now && !pbt0_last) {
            t_start = now;
        }
        /* Detección de flanco de subida (Liberar) */
        else if (!pbt0_now && pbt0_last) {
            uint64_t diff = now - t_start;
            /* 10 MHz -> diff / 10000 para ms */
            uint32_t ms = (uint32_t)(diff / 10000);
            printf("Tiempo pulsado: %u ms\n", ms);

            if (ms > 1000) {
                blink_en = 1;
            }
        }
        pbt0_last = pbt0_now;

        /* Botón 1 para detener el parpadeo */
        if (current_gpio & PBT_1_MASK) {
            blink_en = 0;
            gpio_write(0); /* Apagar LEDs */
        }

        /* Lógica de parpadeo no bloqueante (cada 500ms = 5,000,000 ticks) */
        if (blink_en) {
            if ((now - t_last_blink) >= 5000000) {
                leds_on = !leds_on;
                gpio_write(leds_on ? ALL_LEDS : 0);
                t_last_blink = now;
            }
        }
    }
    return 0;
}
