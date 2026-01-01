#include "riscv_types.h"
#include "riscv_uart.h"
#include "gpio_drv.h"

#include "dispatch.h"
#include "clinc.h"
#include "riscv_monotonic_clock.h"

#include "log.h"


/* ------------------------------------------------------------------ */
/* Configuración de timer y parpadeo                                   */
/* ------------------------------------------------------------------ */
#define GAP_TICKS        (10000u)             /* 1 ms a 10 MHz           */
#define MS_PER_TICK      (1u)                 /* 1 ms por interrupción   */
#define BLINK_HALF_MS    (500u)               /* Parpadeo cada 500 ms    */

/* Máscaras de LEDs (se asume que LED_?_MASK están definidas). */
#define LED_MASK (LED_0_MASK | LED_1_MASK | LED_2_MASK | LED_3_MASK)

/* ------------------------------------------------------------------ */
/* Variables globales usadas en main() e ISR                           */
/* ------------------------------------------------------------------ */
volatile uint32_t counter = 0;
/* Nota: 'counter' se decrementa en la ISR según pide el enunciado.     */

volatile uint32_t ms_now = 0;
/* Reloj de software en milisegundos (res. 1 ms).                       */

volatile uint32_t blink_accum = 0;
/* Acumula ms para conmutar LEDs cada 500 ms cuando parpadean.          */

volatile uint8_t blinking = 0;
/* 0 = sin parpadeo; 1 = parpadeando.                                   */

volatile uint32_t led_out = 0;
/* Imagen de salida solo con bits LED_MASK.                             */

/* ------------------------------------------------------------------ */
/* Rutina de servicio de interrupción del timer                        */
/* ------------------------------------------------------------------ */
void timer_handler(void)
{
    /* Estructura solicitada en el enunciado. */
    if (counter != 0u) {
        counter--;
        if (counter == 0u) {
            /* No se requiere un reseteo específico aquí para la lógica. */
        }
    }

    /* Tictac de software: cada IRQ suma 1 ms. */
    ms_now += MS_PER_TICK;

    /* Gestión del parpadeo sin bloquear la medición de pulsaciones. */
    if (blinking) {
        blink_accum += MS_PER_TICK;

        /* Conmuta LEDs cada 500 ms. */
        if (blink_accum >= BLINK_HALF_MS) {
            blink_accum = 0u;

            /* Toggle de los 4 LEDs: XOR sobre la imagen de salida. */
            led_out ^= LED_MASK;

            /* Aplicar nuevo estado a los LEDs. */
            gpio_write(led_out);
        }
    } else {
        /* Si no hay parpadeo, asegurar que el acumulador no crece. */
        if (blink_accum != 0u) {
            blink_accum = 0u;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Función principal                                                   */
/* ------------------------------------------------------------------ */
int main(void)
{
    uint32_t dir = 0u;
    uint32_t pins = 0u;

    uint8_t btn0_prev = 0u;
    uint8_t btn1_prev = 0u;
    uint8_t btn0_now = 0u;
    uint8_t btn1_now = 0u;

    uint8_t measuring = 0u;
    uint32_t t_start_ms = 0u;
    uint32_t t_end_ms = 0u;
    uint32_t elapsed_ms = 0u;

    /* Configurar como salida exclusivamente los bits 16-19 (LEDs 0-3). */
    dir = LED_MASK;
    gpio_set_direction(dir);

    /* Apagar LEDs al inicio. */
    led_out = 0u;
    gpio_write(led_out);

    /* Instalar y habilitar el timer con el gap de 1 ms (10 000 ticks). */
    install_local_timer_handler(timer_handler);
    local_timer_set_gap(GAP_TICKS);
    enable_timer_clinc_irq();
    enable_irq();

    /* Bucle principal: solo lógica con if-else y lectura de GPIO. */
    while (1) {
        pins = gpio_read();

        /* Supuesto: botones activos a '1'. Cambiar si son activos a '0'. */
        btn0_now = (pins & PBT_0_MASK) ? 1u : 0u;
        btn1_now = (pins & PBT_1_MASK) ? 1u : 0u;

        /* Flanco de subida en botón 0: comienza medición. */
        if ((btn0_prev == 0u) && (btn0_now == 1u)) {
            measuring = 1u;
            t_start_ms = ms_now;
        } else {
            /* Flanco de bajada en botón 0: termina medición. */
            if ((btn0_prev == 1u) && (btn0_now == 0u)) {
                if (measuring == 1u) {
                    measuring = 0u;
                    t_end_ms = ms_now;

                    /* Tiempo pulsado en ms (resolución 1 ms). */
                    if (t_end_ms >= t_start_ms) {
                        elapsed_ms = t_end_ms - t_start_ms;
                    } else {
                        /* Wrap improbable con 32 bits, pero protegido. */
                        elapsed_ms = 0u;
                    }

                    /* Imprime el tiempo pulsado en ms. */
                    printf("Pulsador 0: %u ms\r\n",
                           (unsigned)elapsed_ms);

                    /* Si >= 1 s: encender LEDs y comenzar parpadeo. */
                    if (elapsed_ms >= 1000u) {
                        blinking = 1u;
                        blink_accum = 0u;

                        /* Encender inicialmente los 4 LEDs. */
                        led_out = LED_MASK;
                        gpio_write(led_out);
                    }
                }
            } else {
                /* Sin cambio relevante en botón 0: no hacer nada. */
            }
        }

        /* Flanco de subida en botón 1: detener parpadeo y apagar LEDs. */
        if ((btn1_prev == 0u) && (btn1_now == 1u)) {
            if (blinking == 1u) {
                blinking = 0u;
                blink_accum = 0u;
                led_out = 0u;
                gpio_write(led_out);
            }
        } else {
            /* No hay flanco de subida en botón 1. */
        }

        /* Actualizar memoria de estado de botones. */
        btn0_prev = btn0_now;
        btn1_prev = btn1_now;

        /* Bucle sin bloqueos: la temporización real va en la ISR. */
    }

    /* No se debe llegar aquí. */
    /* return 0; */
}
