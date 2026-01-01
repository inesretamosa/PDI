/* Patrón con interrupciones (IRQ/ISR) para medir tiempos de PBT_0 y
 * controlar parpadeo de LEDs. Requisitos:
 * - Medir e imprimir (ms) entre pulsación y liberación de PBT_0.
 * - Si el tiempo >= 1000 ms: encender LEDs y comenzar parpadeo a 500 ms.
 * - El parpadeo se detiene al pulsar PBT_1.
 * - Mientras parpadea, se puede seguir midiendo con PBT_0.
 * - Usar solo lógica con if-else y las funciones GPIO indicadas.
 * - El contador 'counter' incrementa automáticamente a 10 MHz.
 * - LEDs 0-3 en bits 16-19. Botones 0-3 en bits 4-7.
 * - Máscaras: PBT_0_MASK, PBT_1_MASK, LED_0_MASK..LED_3_MASK.
 * - Líneas <= 80 caracteres.
 */

typedef unsigned char        uint8_t;
typedef unsigned short int   uint16_t;
typedef unsigned int         uint32_t;
typedef unsigned long long   uint64_t;

/* Prototipos de GPIO (proporcionados por la plataforma) */
void gpio_set_direction(uint32_t direction);
void gpio_write(uint32_t output);
uint32_t gpio_read(void);

/* 'counter' es un registro/variable de hardware que incrementa a 10 MHz */
extern volatile uint64_t counter;

/* Para imprimir el tiempo (ajusta según tu plataforma: printf/uart) */
#include <stdio.h>

/* Estados compartidos entre main e ISR (volátiles) */
static volatile uint8_t pbt0_down = 0;
static volatile uint64_t pbt0_press_tick = 0;

static volatile uint8_t blink_on = 0;
static volatile uint64_t next_toggle_tick = 0;
static volatile uint32_t led_state = 0;

/* Conversión: 10 MHz => 10 000 ticks por milisegundo */
#define TICKS_PER_MS (10000ULL)

/* Encender todos los LEDs (16..19) y actualiza 'led_state' */
static void leds_all_on(void)
{
    led_state = LED_0_MASK | LED_1_MASK | LED_2_MASK | LED_3_MASK;
    gpio_write(led_state);
}

/* Apagar todos los LEDs y actualiza 'led_state' */
static void leds_all_off(void)
{
    led_state = 0;
    gpio_write(led_state);
}

/* ISR de GPIO: se llama en cambios de los botones (depende de HW) */
void gpio_isr(void)
{
    uint32_t pins = gpio_read();

    /* PBT_0: medir tiempo entre pulsación y liberación */
    if ((pins & PBT_0_MASK) && (pbt0_down == 0)) {
        /* Flanco de pulsación (press) */
        pbt0_down = 1;
        pbt0_press_tick = counter;
    } else if (!(pins & PBT_0_MASK) && (pbt0_down == 1)) {
        /* Flanco de liberación (release) */
        pbt0_down = 0;

        uint64_t dt_ticks = counter - pbt0_press_tick;
        uint32_t dt_ms = (uint32_t)(dt_ticks / TICKS_PER_MS);

        /* Imprimir tiempo en ms */
        printf("PBT0: %u ms\n", dt_ms);

        /* Si >= 1000 ms: activar parpadeo y encender LEDs */
        if (dt_ms >= 1000U) {
            blink_on = 1;
            leds_all_on();
