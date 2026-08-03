#include "platform.h"
#include "acmconsole.h"

// see config.h
#if defined(HEARTBEAT_LED_PORT) && defined(HEARTBEAT_LED_PIN)

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

static void heartbeat_init(void) {
	rcc_periph_clock_enable(HEARTBEAT_RCC);
	gpio_set_output_options(HEARTBEAT_LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LOW, HEARTBEAT_LED_PIN);
	gpio_mode_setup(HEARTBEAT_LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, HEARTBEAT_LED_PIN);
}

static void heartbeat(uint32_t now) {
	static timeout_t t = { .expired = 1 };
	if(timeout_check(&t,now)) {
		gpio_toggle(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN);
		timeout_set(&t, HZ/2);
	}
}

#else
static void heartbeat_init(void)	{}
static void heartbeat(uint32_t now)	{now=now;}
#endif

#if defined(BREATHING_LED)
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

#define PWM_FREQUENCY	500
#define PWM_MAXVAL		(4096-1)

static void pwmled_init(void) {
    uint32_t prescaler = (rcc_apb1_frequency / (PWM_FREQUENCY * (PWM_MAXVAL+1))) - 1;

	gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_LOW, GPIO1);
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO1);
	gpio_set_af(GPIOB, GPIO_AF0, GPIO1);

	rcc_periph_clock_enable(RCC_TIM14);
	rcc_periph_reset_pulse(RST_TIM14);

	timer_set_prescaler(TIM14, prescaler); // Set prescaler
	timer_set_period(TIM14, PWM_MAXVAL); // Set auto-reload register

	//timer_disable_oc_output(TIM14, TIM_OC1);
	timer_set_oc_mode(TIM14, TIM_OC1, TIM_OCM_PWM1);
	timer_enable_oc_preload(TIM14, TIM_OC1);

	timer_set_oc_value(TIM14, TIM_OC1, 0);
	timer_enable_oc_output(TIM14, TIM_OC1);
	timer_enable_counter(TIM14);
}

static void breathe(void) {
	static uint32_t brightness=0, inc=-32;
	uint32_t pwm;
	if((!brightness) || (brightness == (4096+1024)))
		inc=-inc;
	brightness+=inc;
	pwm = MIN(brightness, 4095);
	pwm *= pwm;
	pwm>>=12;
	timer_set_oc_value(TIM14, TIM_OC1, pwm);
}
#endif /* BREATHING_LED */

int main(void) {
	uint32_t last=0, now;
#if defined(BOOT0_PIN) && defined(BOOT0_PORT)
	uint32_t boot0_trigger = 0;
#endif

	hw_init(); // see ../common-code/platform.c

#ifdef BOOT0_RCC
	rcc_periph_clock_enable(BOOT0_RCC);
#endif

	heartbeat_init();

#ifdef BREATHING_LED
	pwmled_init();
#endif

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

#ifdef BREATHING_LED
		breathe();
#endif
		heartbeat((last=now));
	}
	return 0;
}
