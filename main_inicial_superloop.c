
/*
 * Super-loop en C para medir el tiempo de pulsación de botones y
 * controlar LEDs. Requisitos:
 *  - gpio_read(): lee 32 bits del puerto GPIO (entradas/salidas).
 *  - get_ticks_from_reset(): contador monotónico en ticks (10 MHz).
 *  - Botones 0-3 en bits 4-7 (BTN0 -> bit4, ..., BTN3 -> bit7).
 *  - LEDs 0-3 en bits 16-19 (LED0 -> bit16, ..., LED3 -> bit19).
 *
 * Funcionalidad:
 *  - Detecta la pulsación y la liberación de cada botón con rebote
 *    por software (debounce).
 *  - Imprime el tiempo pulsado en milisegundos.
 *  - Si el tiempo pulsado >= 1000 ms, enciende los 4 LEDs y entra en
 *    modo parpadeo. El parpadeo continúa hasta que se pulse otro
 *    botón distinto al que lo activó.
 *  - Mientras parpadea, se siguen detectando e imprimiendo tiempos
 *    de pulsación de cualquier botón.
 *
 * Notas:
 *  - Se asume botones activos a nivel alto (1 = pulsado). Si son
 *    activos bajos, defina BUTTON_ACTIVE_HIGH como 0.
 *  - Se asume LEDs activos a nivel alto (1 = LED encendido).
 *  - Para escribir el GPIO, se usa gpio_write(value) como función
 *    dependiente de plataforma. Sustituir con la escritura real.
 *  - Líneas limitadas a <= 80 columnas.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/* Prototipos proporcionados por la plataforma. */
uint32_t gpio_read(void);
uint64_t get_ticks_from_reset(void);

/* Sustituir por la escritura real del registro de salida GPIO. */
void gpio_write(uint32_t value);

/* Constantes de mapeo. */
#define BTN_SHIFT             4
#define LED_SHIFT             16
#define BTN_COUNT             4
#define LED_COUNT             4

/* Temporizaciones (ticks y ms). */
#define TICKS_PER_SEC         10000000ULL
#define TICKS_PER_MS          (TICKS_PER_SEC / 1000ULL)
#define DEBOUNCE_MS           20U
#define DEBOUNCE_TICKS        (DEBOUNCE_MS * TICKS_PER_MS)
#define BLINK_PERIOD_MS       250U
#define BLINK_PERIOD_TICKS    (BLINK_PERIOD_MS * TICKS_PER_MS)

/* Nivel activo de los botones: 1 = alto, 0 = bajo. */
#define BUTTON_ACTIVE_HIGH    1

/* Sombra de salida para preservar bits no-LED al escribir GPIO. */
static uint32_t gpio_out_shadow = 0;

/* Estado por botón para debounce y medición. */
typedef struct {
  uint8_t stable;           /* Estado estable: 0 = libre, 1 = pulsado.   */
  uint8_t last_raw;         /* Último valor leído (sin filtrar).         */
  uint64_t last_change;     /* Tick del último cambio de last_raw.       */
  uint8_t waiting_release;  /* 1 si estamos midiendo ese botón.          */
  uint64_t t_press;         /* Tick de flanco de pulsación (estable).    */
} btn_state_t;

/* Utilidad: lee los 4 botones y aplica nivel activo. */
static inline uint8_t read_buttons(uint32_t port)
{
  uint32_t v = (port >> BTN_SHIFT) & 0xF;
#if BUTTON_ACTIVE_HIGH
  return (uint8_t)v;
#else
  return (uint8_t)(~v & 0xF);
#endif
}

/* Escribe patrón de 4 LEDs preservando otros bits del puerto. */
static void set_leds_pattern(uint8_t pat)
{
  gpio_out_shadow &= ~(0xFUL << LED_SHIFT);
  gpio_out_shadow |= ((uint32_t)pat & 0xF) << LED_SHIFT;
  gpio_write(gpio_out_shadow);
}

/* Utilidades LEDs. */
static inline void leds_on_all(void)    { set_leds_pattern(0xF); }
static inline void leds_off_all(void)   { set_leds_pattern(0x0); }
static inline void leds_toggle_all(void)
{
  uint8_t cur = (gpio_out_shadow >> LED_SHIFT) & 0xF;
  set_leds_pattern(cur ^ 0xF);
}

int main(void)
{
  /* Inicialización: tomar estado actual del puerto como sombra. */
  gpio_out_shadow = gpio_read();
  leds_off_all();

  /* Estados de botones. */
  btn_state_t btn[BTN_COUNT] = {0};

  /* Parpadeo: activo, botón origen, timestamp de último toggle. */
  bool blink_active = false;
  uint8_t blink_source = 0xFF;
  uint64_t blink_last = 0;

  /* Bucle principal (super-loop). */
  for (;;) {
    uint64_t now = get_ticks_from_reset();
    uint32_t port = gpio_read();
    uint8_t buttons = read_buttons(port);

    /* Actualizar cada botón con debounce y detectar flancos. */
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
      uint8_t raw = (buttons >> i) & 0x1;

      /* Si cambió el valor crudo, reiniciar temporizador. */
      if (raw != btn[i].last_raw) {
        btn[i].last_raw = raw;
        btn[i].last_change = now;
      }

      /* Si el nuevo valor se mantiene, actualizar estado estable. */
      if ((now - btn[i].last_change) >= DEBOUNCE_TICKS) {
        if (raw != btn[i].stable) {
          btn[i].stable = raw;

          /* Flanco de pulsación (arranque de medición). */
          if (btn[i].stable) {
            btn[i].t_press = now;
            btn[i].waiting_release = 1;

            /* Si parpadea y es otro botón, detener parpadeo. */
            if (blink_active && i != blink_source) {
              blink_active = false;
              leds_off_all();
            }
          } else {
            /* Flanco de liberación: finalizar medición. */
            if (btn[i].waiting_release) {
              uint64_t dt = now - btn[i].t_press;
              uint32_t ms = (uint32_t)(dt / TICKS_PER_MS);
              printf("BTN%u pulsado %u ms\n", i, ms);
              btn[i].waiting_release = 0;

              /* Activar parpadeo si ms >= 1000. */
              if (ms >= 1000U) {
                leds_on_all();
                blink_active = true;
                blink_source = i;
                blink_last = now;
              }
            }
          }
        }
      }
    }

    /* Gestionar parpadeo periódico sin bloquear el super-loop. */
    if (blink_active) {
      if ((now - blink_last) >= BLINK_PERIOD_TICKS) {
        blink_last = now;
        leds_toggle_all();
      }
    }

    /* Opcional: insertar medidas de bajo consumo o espera corta. */
    /* En plataforma real, podría usarse sleep o WFI/WFE si aplica. */
  }

  /* Nunca retorna en un super-loop. */
  return 0;
}
