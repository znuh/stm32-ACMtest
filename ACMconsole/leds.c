#include "config.h"
#include "platform.h"
#include "leds.h"

// see config.h
#if defined(HEARTBEAT_LED_PORT) && defined(HEARTBEAT_LED_PIN)

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

static void heartbeat_init(void) {
	rcc_periph_clock_enable(HEARTBEAT_RCC);
	gpio_set_output_options(HEARTBEAT_LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LOW, HEARTBEAT_LED_PIN);
	gpio_mode_setup(HEARTBEAT_LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, HEARTBEAT_LED_PIN);
}

static void heartbeat(void) {
	static timeout_t t = 0;
	if(timeout(&t)) {
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

	rcc_periph_clock_enable(RCC_GPIOB);
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

void leds_init(void) {
	heartbeat_init();

#ifdef BREATHING_LED
	pwmled_init();
#endif
}

void leds_service(void) {
#ifdef BREATHING_LED
	breathe();
#endif
	heartbeat();
}
