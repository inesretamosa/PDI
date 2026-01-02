#include "riscv_types.h"
#include "riscv_uart.h"
#include "gpio_drv.h"

#include "dispatch.h"
#include "clinc.h"
#include "riscv_monotonic_clock.h"

#include "log.h"

  /* Variables compartidas con la ISR */
  volatile uint32_t ms_ticks = 0;
  volatile uint32_t counter = 0;

  /* Estados del sistema */
  uint8_t btn0_prev = 0;
  uint8_t btn1_prev = 0;
  uint32_t press_time = 0;
  uint32_t blink_timer = 0;
  uint8_t blinking = 0;
  uint8_t leds_on = 0;

  /* ISR del timer: se ejecuta cada 1 ms */
  void timer_handler(void)
  {
      ms_ticks++;

      if (counter > 0)
      {
          counter--;
      }
  }

  /* Programa principal */
  int main(void)
  {
      uint32_t gpio;
      uint32_t elapsed;

      /* Configurar LEDs como salida (bits 16-19) */
      gpio_set_direction(0x000F0000);

      /* Configurar timer*/
      install_local_timer_handler(timer_handler);
      local_timer_set_gap(10000);
      enable_timer_clinc_irq();
      enable_irq();



      while (1)
      {
          gpio = gpio_read();

          /* --- Lectura de pulsador 0 --- */
          if ((gpio & PBT_0_MASK) && !btn0_prev)
          {
              /* Flanco de subida: inicio de pulsación */
              press_time = ms_ticks;
              btn0_prev = 1;
          }
          else if (!(gpio & PBT_0_MASK) && btn0_prev)
          {
              /* Flanco de bajada: fin de pulsación */
              elapsed = ms_ticks - press_time;
              printf("Tiempo pulsado: %u ms\n", elapsed);

              if (elapsed >= 1000)
              {
                  leds_on = 1;
                  blinking = 1;
                  blink_timer = ms_ticks;
              }

              btn0_prev = 0;
          }

          /* --- Control del parpadeo --- */
          if (blinking)
          {
              if ((ms_ticks - blink_timer) >= 500)
              {
                  blink_timer = ms_ticks;

                  if (leds_on)
                  {
                      gpio_write(0x00000000);
                      leds_on = 0;
                  }
                  else
                  {
                      gpio_write(0x000F0000);
                      leds_on = 1;
                  }
              }
          }

          /* --- Lectura de pulsador 1 --- */
          if ((gpio & PBT_1_MASK) && !btn1_prev)
          {
              /* Detener parpadeo */
              blinking = 0;
              leds_on = 0;
              gpio_write(0x00000000);
              btn1_prev = 1;
          }
          else if (!(gpio & PBT_1_MASK))
          {
              btn1_prev = 0;
          }
      }
  }
