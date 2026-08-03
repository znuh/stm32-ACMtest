#include "platform.h"
#include "acmconsole.h"
#include "leds.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

int main(void) {
	uint32_t last=0, now;
#if defined(BOOT0_PIN) && defined(BOOT0_PORT)
	uint32_t boot0_trigger = 0;
#endif

	hw_init(); // see ../common-code/platform.c

#ifdef BOOT0_RCC
	rcc_periph_clock_enable(BOOT0_RCC);
#endif

	leds_init();

	acmconsole_init();

	/* main loop */
	while(1) {
		SLEEP_UNTIL((last != (now=jiffies)) || ACM_rx_fill);

		if(ACM_rx_fill)
			ACM_to_console();

		if(last == now)
			continue;

#if defined(BOOT0_PIN) && defined(BOOT0_PORT)
		/* check BOOT0 */
		if(gpio_get(BOOT0_PORT, BOOT0_PIN)) {
			if(++boot0_trigger == (HZ<<1)) {
				SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ; /* trigger system reset via SCB */
				while(1) {}
			}
		}
		else
			boot0_trigger = 0;
#endif /* BOOT0 */

		leds_service();
	}
	return 0;
}
