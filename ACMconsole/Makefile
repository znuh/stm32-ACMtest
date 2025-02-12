PROJECT = ACMconsole
BUILD_DIR = bin

SHARED_DIR = ../common-code
CFILES = main.c
CFILES += console.c platform.c stm32f0_usb.c
AFILES += lowlevel.S

# TODO - you will need to edit these two lines!
DEVICE=stm32f042k6t6
#OOCD_FILE = board/stm32f4discovery.cfg

# You shouldn't have to edit anything below here.
VPATH += $(SHARED_DIR)
INCLUDES += $(patsubst %,-I%, . $(SHARED_DIR))
OPENCM3_DIR=../libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

all: $(PROJECT).bin
	$(PREFIX)size $(PROJECT).elf

load: $(PROJECT).bin
	(echo ICANHAZBOOTLOADER > /dev/ttyACM0 && sleep 2) || true
	dfu-util -a 0 -s 0x08000000:leave -D $(PROJECT).bin
