#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
#include <stddef.h>

#include "config.h"

#include <libopencmsis/core_cm3.h>
#include "utils.h"
#include "lowlevel.h"

/* checks condition with IRQs disabled
 * WARNING: leaves IRQs disabled - you must enable them manually again */
#define SLEEP_UNTIL_IRQDISABLE(cond) for(__disable_irq(); !(cond); ) {								\
	/* WFI sleeps until 'An interrupt becomes pending which would preempt if PRIMASK was clear' */	\
	__WFI();																						\
	__enable_irq();		/* run ISR */																\
	__disable_irq();																				\
}

#define SLEEP_UNTIL(cond) do { SLEEP_UNTIL_IRQDISABLE(cond); __enable_irq(); } while(0)

/* simple core delay loop w/o sleeping */
//#define delay_cycles(n)   delay_loop((n)>>2)   // no need for +1 because there's always 5 extra cycles anyway
#define delay_us(n)       delay_loop((n)*16)
#define delay_ms(n)       delay_loop((n)*16000)

#define HZ                100
#define MSEC             (1000/HZ)
#define SEC              (1000*MSEC)
#define MS_TO_TICKS(a)   (((a)+(MSEC-1))/MSEC)

extern volatile uint64_t _jiffies;

static inline uint64_t jiffies_rd(void) {
	__asm__ volatile ("CPSID I" : : : "memory");
	uint64_t res = _jiffies;
	__asm__ volatile ("CPSIE I" : : : "memory");
	return res;
}

typedef uint64_t timeout_t;

/* systick based timeouts / sleep functions */

static inline void timeout_set(timeout_t *to, uint32_t ticks) {
	uint64_t v = jiffies_rd();
	v+=ticks;
	*to=v;
}

static inline int timeout_check(timeout_t *to, uint64_t now) {
	return now >= *to;
}

#define timeout(to) timeout_check(to, jiffies_rd())

static inline void timeout_sleep(timeout_t *to) {
	uint64_t last;
	for(last=jiffies_rd(); !timeout_check(to, last); last=jiffies_rd())
		SLEEP_UNTIL(jiffies_rd() != last);
}

#define sleep_ms(ms) do { timeout_t _to; timeout_set(&_to, MS_TO_TICKS(ms)); timeout_sleep(&_to); } while(0)

void hw_init(void);

extern volatile uint32_t ACM_rx_fill;

extern volatile uint32_t SIGINT;

int  ACM_tx(const void *p, size_t n, int ascii);
void ACM_waitfor_txdone(void);
void ACM_to_console(void);
int  ACM_readbyte(void);
void usb_shutdown(void);

#define BOOTLOADER_MAGIC	0xDEADBEEF
void system_reset(uint32_t bl_magic);

extern uint32_t hardfault_dump[9];
#define HARDFAULT_MAGIC		0xDEADBEEF

#endif /* PLATFORM_H */
