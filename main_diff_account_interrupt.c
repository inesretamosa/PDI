#include "riscv_types.h"
#include "riscv_uart.h"
#include "gpio_drv.h"

#include "dispatch.h"
#include "clinc.h"
#include "riscv_monotonic_clock.h"

#include "log.h"


      /* Contador decreciente */
      volatile uint64_t counter_ms = (uint64_t)-1;


      /* Timer ISR: decrementa la variable global counter_ms cada ms.
               Se asume que local_timer_set_gap(10000) genera una IRQ cada 1 ms
               porque el reloj interno incrementa a 10 MHz. */
      void timer_handler(void)
      {
          /* Decrementar hasta llegar a 0, tal como se pide */
          if (counter_ms > 0) {
              counter_ms--;
          } else {
              /* si llegara a 0, se queda en 0 (no underflow) */
              counter_ms = 0;
          }
      }

      /* Main: mide tiempo entre pulsación y liberación de botón 0,
         enciende LEDs si > 1000 ms y hace parpadeo cada 500 ms hasta
         que se pulse el botón 1. El resto del programa es un super-loop. */
      int main(void)
      {
          /* Bits de LEDs 0-3 están en posiciones 16-19 */
          const uint32_t LED_MASK = (0xF << 16);

          /* Variables de estado */
          volatile uint64_t start_cnt = 0;
          volatile uint64_t end_cnt = 0;
          volatile uint8_t measuring = 0;    /* true mientras se mantiene pulsado */
          volatile uint8_t leds_on = 0;      /* true si LEDs encendidos por >1s */
          volatile uint8_t blinking = 0;     /* true si en modo parpadeo */
          volatile uint32_t led_output = 0;   /* valor a escribir en GPIO */
          uint32_t gpio_dir = LED_MASK;       /* configurar solo LEDs como salida */

          /* Variables para detección de flancos y parpadeo */
          uint32_t prev_gpio = 0;
          uint32_t cur_gpio = 0;
          uint8_t prev_btn0 = 0;
          uint8_t prev_btn1 = 0;
          uint64_t last_blink_toggle_cnt = 0;
          const uint64_t BLINK_PERIOD_MS = 500;

          /* Inicializaciones hardware */
          gpio_set_direction(gpio_dir);       /* LEDs como salida */
          gpio_write(0);                      /* apagar LEDs inicialmente */

          /* Configurar timer para 1 ms: gap = 10000 ticks (10 MHz clock) */
          install_local_timer_handler(timer_handler);
          local_timer_set_gap(10000);
          enable_timer_clinc_irq();
          enable_irq();

          /* Inicializar contador decreciente muy grande para evitar 0 pronto */
          counter_ms = (uint64_t)0xFFFFFFFFFFFFFFFFULL;

          /* Leer estado inicial de botones */
          prev_gpio = gpio_read();
          prev_btn0 = (prev_gpio & PBT_0_MASK) ? 1 : 0;
          prev_btn1 = (prev_gpio & PBT_1_MASK) ? 1 : 0;

          /* Super-loop principal */
          while (1) {
              /* Leer GPIO (botones) */
              cur_gpio = gpio_read();
              /* Extraer estados booleanos */
              uint8_t cur_btn0 = (cur_gpio & PBT_0_MASK) ? 1 : 0;
              uint8_t cur_btn1 = (cur_gpio & PBT_1_MASK) ? 1 : 0;

              /* --- Lógica de botón 0: detectar flancos con if-else --- */
              if (!prev_btn0 && cur_btn0) {
                  /* Flanco de subida: botón 0 acaba de ser pulsado */
                  measuring = 1;
                  /* Guardar contador en el instante de pulsar */
                  start_cnt = counter_ms;
              } else {
                  if (prev_btn0 && !cur_btn0) {
                      /* Flanco de bajada: botón 0 acaba de ser liberado */
                      if (measuring) {
                          measuring = 0;
                          /* Guardar contador en el instante de liberar */
                          end_cnt = counter_ms;
                          /* Calcular tiempo transcurrido en ms.
                             Como counter_ms es decreciente, start_cnt >= end_cnt */
                          uint64_t elapsed_ms = 0;
                          if (start_cnt >= end_cnt) {
                              elapsed_ms = start_cnt - end_cnt;
                          } else {
                              /* Caso improbable de wrap; manejar como 0 */
                              elapsed_ms = 0;
                          }
                          /* Imprimir tiempo en ms */
                          printf("Tiempo pulsado: %u ms\n", (unsigned)elapsed_ms);

                          /* Si supera 1000 ms, encender LEDs y activar parpadeo */
                          if (elapsed_ms > 1000) {
                              leds_on = 1;
                              blinking = 1;
                              /* Encender LEDs inmediatamente */
                              led_output = LED_MASK;
                              gpio_write(led_output);
                              /* inicializar toggle timer */
                              last_blink_toggle_cnt = counter_ms;
                          } else {
                              /* Si no supera 1s, no cambiar modo parpadeo */
                              /* Mantener estado previo de blinking/leds */
                          }
                      }
                  } else {
                      /* No hay flanco en botón 0: nada que hacer aquí */
                  }
              }

              /* --- Lógica de botón 1: detener parpadeo al pulsar --- */
              if (!prev_btn1 && cur_btn1) {
                  /* Flanco de subida en botón 1: detener parpadeo y apagar LEDs */
                  if (blinking || leds_on) {
                      blinking = 0;
                      leds_on = 0;
                      led_output = 0;
                      gpio_write(led_output);
                  }
              } else {
                  /* No flanco en botón 1: nada que hacer */
              }

              /* --- Si estamos en modo parpadeo, alternar cada 500 ms --- */
              if (blinking) {
                  /* Calcular tiempo transcurrido desde último toggle */
                  uint64_t elapsed_since_toggle = 0;
                  if (last_blink_toggle_cnt >= counter_ms) {
                      elapsed_since_toggle = last_blink_toggle_cnt - counter_ms;
                  } else {
                      /* wrap improbable: resetar referencia */
                      last_blink_toggle_cnt = counter_ms;
                      elapsed_since_toggle = 0;
                  }

                  if (elapsed_since_toggle >= BLINK_PERIOD_MS) {
                      /* Toggle LEDs */
                      if (led_output & LED_MASK) {
                          led_output &= ~LED_MASK; /* apagar LEDs */
                      } else {
                          led_output |= LED_MASK;  /* encender LEDs */
                      }
                      gpio_write(led_output);
                      /* actualizar referencia de toggle */
                      last_blink_toggle_cnt = counter_ms;
                  } else {
                      /* No es tiempo de toggle aún */
                  }
              } else {
                  /* No en parpadeo: si leds_on es true, mantener encendidos */
                  if (leds_on) {
                      /* Asegurar LEDs encendidos */
                      if ((led_output & LED_MASK) == 0) {
                          led_output |= LED_MASK;
                          gpio_write(led_output);
                      }
                  } else {
                      /* Asegurar LEDs apagados */
                      if (led_output & LED_MASK) {
                          led_output &= ~LED_MASK;
                          gpio_write(led_output);
                      }
                  }
              }

              /* Actualizar estados previos para detección de flancos */
              prev_btn0 = cur_btn0;
              prev_btn1 = cur_btn1;
              prev_gpio = cur_gpio;

              /* Super-loop ligero: se puede añadir WFI o sleep si la plataforma
                 lo permite; aquí se mantiene activo para permitir lecturas
                 frecuentes y que el timer ISR actualice counter_ms. */
          }

          /* Nunca se llega aquí */
          return 0;
      }
