#include "platform.h"
#include "console.h"

#include <string.h>

#ifndef NO_STDIO
#include <stdio.h>
#include <unistd.h>
#else
#define fflush(a)
#define stdout
#define write(fd,p,n)   ACM_tx((p), (n), 1)
#define fputs(str,fh)   ACM_tx((str), strlen(str), 1)
#define puts(str)       do{ ACM_tx((str), strlen(str), 1); ACM_tx("\n", 1, 1); } while(0)
#endif // defined(NO_STDIO)

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

/* these are some example console commands
 *
 * make sure to have a look at common-code/console_config.h to verify the console settings */

CONSOLE_COMMAND_DEF(ver, "show firmware info/version");
static void ver_command_handler(void) {
	puts("FW_BASE: "GIT_VERSION"\n"__FILE__" "__DATE__" "__TIME__);
}

CONSOLE_COMMAND_DEF(erase_vt, "erase flash page 0 to reenable DFU bootloader");
static void erase_vt_command_handler(void) {
	const char *yes = "yes\n";
	fputs("WARNING! This will erase a part of the firmware (flash page 0) to reenable\n"
	"the BootROM DFU bootloader. The firmware will no longer work after this!\n"
	"A powercycle may be needed to access the bootloader.\n"
	"Continue? (Enter yes): ", stdout);
	fflush(stdout);
	ACM_waitfor_txdone();
	for(SIGINT=0;*yes;yes++) {
		int v = ACM_readbyte();
		if((*yes ^ v) || SIGINT)
			goto abort;
		write(1,&v,1);
	}
	if((!(*yes)) && (!SIGINT)) {
		puts("Erasing page 0 - USB will disconnect now.");
		fflush(stdout);
		ACM_waitfor_txdone();
		usb_shutdown();
		erase_page0(0xAA55);
	}
	return;
abort:
	puts("\nuser abort");
}

CONSOLE_COMMAND_DEF(anim, "nonsense command to demonstrate SIGINT & WFI sleeping");
static void anim_command_handler(void) {
	const char seq[] = "\r.\ro\rO\ro";
	int idx=0;
	puts("demo loop - abort with Ctrl+C");
	// SIGINT is set in USB rx handler ISR - must be cleared by user
	for(SIGINT=0;!SIGINT;idx+=2) {
		write(1,seq+(idx&7),2);
		sleep_ms(100); // this will not abort on SIGINT - use SLEEP_UNTIL(timeout_check(...) || SIGINT) for interruptible sleeping
	}
	puts("");
}

/* example command with arguments */
CONSOLE_COMMAND_DEF(echo, "example command - takes one integer and an optional string argument",
	CONSOLE_INT_ARG_DEF(arg1, "integer argument"),
	CONSOLE_OPTIONAL_STR_ARG_DEF(str, "optional string argument")
);
static void echo_command_handler(const echo_args_t* args) {
	char buf[20] = "arg1: ";
	i32_to_dec(args->arg1, buf+6, 11, -1, 0);
	fputs(buf, stdout);
	fputs(", arg2: ", stdout);
	puts(args->str ? args->str : "(NULL)");
}

/* list of console commands */
static const console_command_def_t * const console_commands[] = {
	ver, erase_vt, anim, echo,
	NULL
};

/* write function for console */
static void console_write(const char *s) {
	int len=strlen(s);
	write(1,s,len);
}

#ifdef STM32C0
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#endif

int main(void) {
	const console_init_t init_console = {.write_function = console_write};
	const console_command_def_t * const *cmd;
	uint32_t last=0;

#ifdef STM32C0
	rcc_periph_clock_enable(RCC_GPIOB);
	gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO9);
#endif

	hw_init(); // see ../common-code/platform.c

	heartbeat_init();

	/* init console & register all commands */
	console_init(&init_console);
	for(cmd=console_commands;*cmd;cmd++)
		console_command_register(*cmd);

	/* main loop */
	while(1) {
#ifdef STM32C0
		if(gpio_get(GPIOB, GPIO9))
			erase_page0(0xAA55);
#endif
		//SLEEP_UNTIL(ACM_rx_fill || (last != jiffies));
		//if(ACM_rx_fill)
			//ACM_to_console();
		heartbeat((last=jiffies));
		usb_isr();
	}
	return 0;
}
