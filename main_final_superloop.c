#include "riscv_types.h"
#include "riscv_uart.h"
#include "gpio_drv.h"

#include "dispatch.h"
#include "clinc.h"
#include "riscv_monotonic_clock.h"

#include "log.h"

#define TICKS_PER_MS  (CLINT_CLOCK / 1000U)
#define LEDS_ALL (LED_0_MASK|LED_1_MASK|LED_2_MASK|LED_3_MASK)
#define BLINK_MS   500U
#define BLINK_TCK  (BLINK_MS * TICKS_PER_MS)

int main(void)
{
  gpio_set_direction(LEDS_ALL);

  uint32_t out_shadow = gpio_read();
  out_shadow &= ~LEDS_ALL;
  gpio_write(out_shadow);

  uint32_t input_previous = gpio_read();
  uint32_t input_current  = input_previous;
  uint32_t button_0_prev  = input_previous & PBT_0_MASK;
  uint32_t button_0_curr  = button_0_prev;
  uint32_t button_1_prev  = input_previous & PBT_1_MASK;
  uint32_t button_1_curr  = button_1_prev;

  uint8_t  b0_measuring = 0;
  uint64_t t_press = 0;

  uint8_t  blink = 0;
  uint64_t blink_last = 0;

  while (1) {
    uint64_t now = get_ticks_from_reset();

    input_previous = input_current;
    input_current  = gpio_read();

    button_0_prev  = input_previous & PBT_0_MASK;
    button_0_curr  = input_current  & PBT_0_MASK;

    button_1_prev  = input_previous & PBT_1_MASK;
    button_1_curr  = input_current  & PBT_1_MASK;

    /* Flanco subida BTN0: iniciar medida. */
    if (!button_0_prev && button_0_curr) {
      b0_measuring = 1;
      t_press = now;
    }

    /* Flanco bajada BTN0: finalizar medida. */
    if (button_0_prev && !button_0_curr) {
      if (b0_measuring) {
        b0_measuring = 0;
        uint64_t dt = now - t_press;
        uint32_t ms = (uint32_t)(dt / TICKS_PER_MS);
        printf("BTN0 pulsado %u ms\n", ms);
        if (ms >= 1000U) {
          out_shadow |= LEDS_ALL;
          gpio_write(out_shadow);
          blink = 1;
          blink_last = now;
        }
      }
    }

    /* Flanco subida BTN1: detener parpadeo. */
    if (!button_1_prev && button_1_curr) {
      if (blink) {
        blink = 0;
        out_shadow &= ~LEDS_ALL;
        gpio_write(out_shadow);
      }
    }

    /* Parpadeo no bloqueante. */
    if (blink && (now - blink_last) >= BLINK_TCK) {
      blink_last = now;
      if (out_shadow & LEDS_ALL) out_shadow &= ~LEDS_ALL;
      else                       out_shadow |=  LEDS_ALL;
      gpio_write(out_shadow);
    }
  }

  return 0;
}
