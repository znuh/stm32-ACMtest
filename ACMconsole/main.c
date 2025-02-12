#include "platform.h"

int main(void) {
	hw_init();
	while(1) {
		SLEEP_UNTIL(0);
	}
}
