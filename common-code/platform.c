#include "platform.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/syscfg.h>
#include <libopencm3/stm32/flash.h>

#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include <stdint.h>
#include <inttypes.h>

extern void usb_setup(void);

static void gpio_setup(void) {
}

volatile uint32_t jiffies = 0;

void sys_tick_handler(void) {
	jiffies++;
}

void timeout_set(timeout_t *to, uint32_t ticks) {
	to->start = jiffies;
	to->expired = ticks == 0;
	to->end = to->start + ticks + 1;          /* need to add 2 timer cycles b/c current cycle already started */
	to->need_rollover = to->start >= to->end; /* ticks is at least 1 so equal case means a rollover too */
	to->rollover = 0;
}

int timeout_check(timeout_t *to, uint32_t now) {
	to->rollover |= now < to->start;
	to->expired  |= ((now >= to->end) && (to->rollover >= to->need_rollover)) || (to->rollover > to->need_rollover);
	return to->expired;
}

void timeout_sleep(timeout_t *to) {
	uint32_t last;
	for(last=jiffies; !timeout_check(to, last); last=jiffies)
		SLEEP_UNTIL(last != jiffies);
}

/*
void sleep_ms(uint32_t ms) {
	timeout_t to;
	timeout_set(&to, MS_TO_TICKS(ms));
	timeout_sleep(&to);
}
*/

static void clocks_setup(void) {
	rcc_clock_setup_in_hsi48_out_48mhz();
//	rcc_periph_clock_enable(RCC_GPIOA);
//	rcc_periph_clock_enable(RCC_GPIOB);
}

static void systick_setup(void) {
	systick_set_frequency(HZ, rcc_ahb_frequency);
	systick_clear();
	systick_counter_enable();
	nvic_set_priority(NVIC_SYSTICK_IRQ, 255);  // lowest priority
	systick_interrupt_enable();
}

static void __attribute__( (section(".data#"), long_call, noinline) ) erase0_ram_func(void) {   /* extra # after section name mutes the asm warning m) */
	FLASH_CR |= FLASH_CR_PER;
	FLASH_AR = 0x08000000; /* erase the page of the vetor table to enforce bootloader mode */
	FLASH_CR |= FLASH_CR_STRT;

	while(FLASH_SR & FLASH_SR_BSY) {} /* busywait */
	FLASH_CR &= ~FLASH_CR_PER;

	SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ; /* trigger system reset via SCB */
	while(1) {}
}

void erase_page0(uint32_t safety_key) {
	if(safety_key != 0xAA55) {
		SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ; /* trigger system reset via SCB */
		while(1) {}
		return;
	}
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO11 | GPIO12);
	delay_ms(200);                /* wait a bit after USB disconnect - msleep would require the timer interrupt */
	cm_disable_interrupts();
	flash_unlock();
	flash_wait_for_last_operation();
	erase0_ram_func();
}

void hw_init(void) {
	clocks_setup();
	systick_setup();
	gpio_setup();
	usb_setup();
}
