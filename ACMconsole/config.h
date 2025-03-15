#ifndef CONFIG_H
#define CONFIG_H

/* see common-code/console_config.h for console configuration */

/* You can disable STDIO to save ~3.5kB of flash space. (f)puts/printf, etc.
 * and unistd write won't work then.
 * If you use STDIO and want to mix STDIO and unbuffered raw IO, be sure to
 * use fflush, or things will go awry */
//#define NO_STDIO

/* you can enable a heartbeat LED here - only active in main loop */

#define HEARTBEAT_RCC 			RCC_GPIOB
#define HEARTBEAT_LED_PORT		GPIOB

#if defined(STM32C0)
#define HEARTBEAT_LED_PIN		GPIO2	/* cyan */
#define BREATHING_LED

#define BOOT0_RCC				RCC_GPIOA
#define BOOT0_PORT				GPIOA
#define BOOT0_PIN				GPIO14

#elif defined(STM32F0)
#define HEARTBEAT_LED_PIN		GPIO8

#define BOOT0_RCC				RCC_GPIOB
#define BOOT0_PORT				GPIOB
#define BOOT0_PIN				GPIO88
#endif



#endif
