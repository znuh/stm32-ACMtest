#include "platform.h"
#include "console.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>

CONSOLE_COMMAND_DEF(ver, "show firmware info/version");
static void ver_command_handler(void) {
	puts("FW_BASE: "GIT_VERSION"\n"__FILE__" "__DATE__" "__TIME__);
}

static const console_command_def_t * const console_commands[] = {
	ver,
	NULL
};

static void console_write(const char *s) {
	int len=strlen(s);
	write(1,s,len);
}

int main(void) {
	const console_init_t init_console = {.write_function = console_write};
	const console_command_def_t * const *cmd;

	hw_init();

	console_init(&init_console);
	for(cmd=console_commands;*cmd;cmd++)
		console_command_register(*cmd);

	while(1) {
		SLEEP_UNTIL(usb_rx_fill);
		if(usb_rx_fill)
			usb_to_console();
	}
	return 0;
}
