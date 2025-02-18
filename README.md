This is a skeleton project with a USB ttyACM device for STM32F0 MCUs.
It is based on the libopencm3-template repository (https://github.com/libopencm3/libopencm3-template.git)

Easy "clone and go" repository for a libopencm3-based STM32F0 project with an ACM console via USB.

# Notes
* no external HSE crystal is needed for USB ACM operation, because the internal HSI48 oscillator with the Clock Recovery System (CRS) is used.

# Getting & Compiling the Code
 1. git clone --recurse-submodules https://github.com/znuh/stm32-ACMconsole.git your-project
 2. cd your-project
 3. make -C libopencm3 # (Only needed once)
 4. make -C ACMconsole

If you have an older git, or got ahead of yourself and skipped the ```--recurse-submodules```
you can fix things by running ```git submodule update --init``` (This is only needed once)

# Directories
* ACMconsole contains the skeleton ttyACM application
* common-code contains USB handling, platform-specific code and the console implementation

# Memory Usage
* the base code for the ACM USB device makes up ~7.7 kB (code in flash memory)
* the console (with history, help, tab completion, etc.) on top adds ~8.3 kB (code in flash memory)
=> you need a STM32F0 with >= 32 kB flash memory to use this

* BSS usage is ~3.6 kB - can be reduced by shrinking the (1 KiB) USB ACM TX buffer
