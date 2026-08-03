#include "acmconsole.h"
#include "console.h"
#include "config.h"
#include "platform.h"

#include <string.h>

// for mem dump
#include <stdlib.h>
#include "utils.h"

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

/* these are some example console commands
 *
 * make sure to have a look at common-code/console_config.h to verify the console settings */

CONSOLE_COMMAND_DEF(ver, "show firmware info/version");
static void ver_command_handler(void) {
	puts("FW_BASE: "GIT_VERSION"\n"__FILE__" "__DATE__" "__TIME__);
}

#include <libopencm3/stm32/desig.h>

CONSOLE_COMMAND_DEF(sn, "show serial number");
static void sn_command_handler(void) {
	char serial_nr[16];
	desig_get_unique_id_as_dfu(serial_nr);
	puts(serial_nr);
}

CONSOLE_COMMAND_DEF(md, "memory dump (32Bit words)",
	CONSOLE_STR_ARG_DEF(addr, "hex address"),
	CONSOLE_OPTIONAL_INT_ARG_DEF(n, "n_words")
);
static void md_command_handler(const md_args_t* args) {
	uint32_t addr = strtoul(args->addr, NULL, 16);
	volatile uint32_t *src = (volatile uint32_t *)(addr & (~3));
	uint32_t i, n = (args->n >= 1) ? args->n : 8;
	char buf[16];
	for(i=0;i<n;i++,src++) {
		/* print addr */
		if(!(i&7)) {
			u32_to_hex((uint32_t)src, buf);
			buf[8]=':';
			buf[9]=' ';
			buf[10]=0;
			fputs(buf, stdout);
		}
		u32_to_hex(*src, buf);
		buf[8]= ((i&7)==7) ? '\n' : ' ';
		buf[9]=0;
		fputs(buf, stdout);
	}
	if(i&7)
		puts("");
}

CONSOLE_COMMAND_DEF(reset, "trigger system reset",
	CONSOLE_OPTIONAL_STR_ARG_DEF(bl, "\"bl\" for bootloader")
);
static void reset_command_handler(const reset_args_t* args) {
	uint32_t bl_magic = 0;
	if(args->bl && !strcmp(args->bl,"bl"))
		bl_magic = BOOTLOADER_MAGIC;
	system_reset(bl_magic);
}

CONSOLE_COMMAND_DEF(hf, "dump hardfault regs / test hardfault",
	CONSOLE_OPTIONAL_STR_ARG_DEF(opt, "clear OR test")
);
static void hf_command_handler(const hf_args_t* args) {
	static const char rnames[8][4] = {"r0", "r1", "r2", "r3", "r12", "lr", "pc", "psr"};
	if(hardfault_dump[8] != HARDFAULT_MAGIC)
		return;
	puts("HardFault Dump:");
	for(int i=0;i<8;i++)
		printf("%3s %08"PRIx32"\n",rnames[i],hardfault_dump[i]);
	if(args->opt) {
		 if(!strcmp(args->opt,"clear")) {
			hardfault_dump[8]=0;
			puts("Dump cleared.");
		}
		else if(!strcmp(args->opt,"test")) {
			__asm volatile ("udf #0");
		}
	}
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

#include <libopencm3/stm32/flash.h>

CONSOLE_COMMAND_DEF(boot_sel, "Set BOOT_SEL OPTION Bit",
	CONSOLE_OPTIONAL_INT_ARG_DEF(val, "0/1")
);
static void boot_sel_command_handler(const boot_sel_args_t* args) {
	/* Option Bytes are read directly from Flash because FLASH_OPTR register
	 * does not change its value until a POR occurs */
	uint32_t new, old = FLASH_OPTION_BYTES;
	char buf[16]="01234567 (x),";

	fputs("current val: ", stdout);
	u32_to_hex(old, buf);
	buf[8]  = ' ';
	buf[10] = '0' + !!(old&FLASH_OPTR_nBOOT_SEL);
	buf[12] = (args->val < 0) ? '\n' : ',';
	fputs(buf, stdout);

	if(args->val < 0)
		return;

	new = old & ~FLASH_OPTR_nBOOT_SEL;
	new |= args->val ? FLASH_OPTR_nBOOT_SEL : 0;

	fputs(" new val: ", stdout);
	u32_to_hex(new, buf);
	buf[8]  = ' ';
	buf[10] = '0' + !!(new&FLASH_OPTR_nBOOT_SEL);
	buf[12] = '\n';
	fputs(buf, stdout);

	if(new != old) {
		ACM_waitfor_txdone();
		flash_program_option_bytes(new);
		fputs("val after write: ", stdout);
		new = FLASH_OPTION_BYTES;
		u32_to_hex(new, buf);
		buf[8]  = ' ';
		buf[10] = '0' + !!(new&FLASH_OPTR_nBOOT_SEL);
		fputs(buf, stdout);
		puts("NOTE: Power-cycle needed to apply new configuration!");
	}
}

/* list of console commands */
static const console_command_def_t * const console_commands[] = {
	ver, sn, md, hf, anim, echo,
	boot_sel, reset,
	NULL
};

/* write function for console */
static void console_write(const char *s) {
	int len=strlen(s);
	write(1,s,len);
}

static const console_init_t init_console = {.write_function = console_write};

void acmconsole_init(void) {
	const console_command_def_t * const *cmd;

	/* init console & register all commands */
	console_init(&init_console);
	for(cmd=console_commands;*cmd;cmd++)
		console_command_register(*cmd);
}
