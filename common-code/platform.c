#include "platform.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/syscfg.h>

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

uint32_t __attribute__((section(".noinit"))) hardfault_dump[9];

static uint32_t __attribute__((section(".noinit"))) boot_magic;

static void __attribute__((constructor)) early_init(void) {
	if(boot_magic != BOOTLOADER_MAGIC)
		return;

	boot_magic = 0;
	__asm__ volatile ("CPSID I\n");

	uint32_t *vtab = (uint32_t *)0x1FFF0000;
	SCB_VTOR = (uint32_t)vtab;
	__asm__ volatile (
        "ldr r1, [%0]\n"
        "msr msp, r1\n"
        "ldr %0, [%0, #4]\n"
        "bx %0\n"
        : "+r" (vtab) : : "r1", "memory"
    );
}

void system_reset(uint32_t bl_magic) {
	usb_shutdown();
	boot_magic = bl_magic;
	__asm__ volatile ("dsb\n");
	SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ; /* trigger system reset via SCB */
	while(1){}
}

void hw_init(void) {
	/* do a clean reset if the bootloader was running before */
	if(SCB_VTOR)
		SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ;

	clocks_setup();

	/* Enable Boot0 pin in Option Bytes? */
#if defined(STM32C0) && defined(BOOT0_PIN_ENABLE)
	/* Option Bytes are read directly from Flash because FLASH_OPTR register
	 * does not change its value until a POR occurs */
	if(FLASH_OPTION_BYTES & FLASH_OPTR_nBOOT_SEL)
		flash_program_option_bytes(FLASH_OPTION_BYTES & (~FLASH_OPTR_nBOOT_SEL));
#endif

	systick_setup();
	gpio_setup();
	usb_setup();
}
