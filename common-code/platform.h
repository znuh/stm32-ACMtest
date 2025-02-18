#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
#include <stddef.h>

#include <libopencmsis/core_cm3.h>
#include "utils.h"
#include "lowlevel.h"

/* simple core delay loop w/o sleeping */
#define delay_cycles(n)   delay_loop((n)>>2)   // no need for +1 because there's always 5 extra cycles anyway
#define delay_us(n)       delay_loop((n)*12)
#define delay_ms(n)       delay_loop((n)*12000)

#define HZ                100
#define MSEC             (1000/HZ)
#define SEC              (1000*MSEC)
#define MS_TO_TICKS(a)   (((a)+(MSEC-1))/MSEC)

extern volatile uint32_t jiffies;

typedef struct timeout_s {
	uint32_t start;
	uint32_t end;
	int need_rollover;
	int expired;
	int rollover;
} timeout_t;

/* systick based timeouts / sleep functions */
void    timeout_set(timeout_t *to, uint32_t ticks);
int     timeout_check(timeout_t *to, uint32_t now);
#define timeout(to) timeout_check(to, jiffies)
void    timeout_sleep(timeout_t *to);
//void    sleep_ms(uint32_t ms);
#define sleep_ms(ms) do { timeout_t to; timeout_set(&to, MS_TO_TICKS(ms)); timeout_sleep(&to); } while(0)

void hw_init(void);

/* checks condition with IRQs disabled
 * WARNING: leaves IRQs disabled - you must enable them manually again */
#define SLEEP_UNTIL_IRQDISABLE(cond) for(__disable_irq(); !(cond); ) {								\
	/* WFI sleeps until 'An interrupt becomes pending which would preempt if PRIMASK was clear' */	\
	__WFI();																						\
	__enable_irq();		/* run ISR */																\
	__disable_irq();																				\
}

#define SLEEP_UNTIL(cond) do { SLEEP_UNTIL_IRQDISABLE(cond); __enable_irq(); } while(0)

extern volatile uint32_t usb_rx_fill;

extern volatile uint32_t SIGINT;

void cdcacm_waitfor_txdone(void);
void usb_to_console(void);
int usb_readbyte(void);
void usb_shutdown(void);

#endif /* PLATFORM_H */
