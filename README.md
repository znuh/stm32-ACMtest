This is a USB ACM demo / template project for STM32F0 MCUs. The ACM device provides a fancy console with tab completion, history, help, etc.
It is based on the libopencm3-template repository (https://github.com/libopencm3/libopencm3-template.git) and the anchor console implementation (https://github.com/rideskip/anchor/tree/master/console).

Easy "clone and go" repository for a libopencm3-based STM32F0 project with an ACM console via USB.

# Features
* crystal-less USB operation (using the Clock Recovery System (CRS))
**=> no external HSE crystal is needed for USB**
* USB ACM device
* ACM console with tab completion, history, help, args checking, etc.
* systick timer
* WFI based sleeping
* heartbeat LED (optional, disabled by default - check config.h)

# Getting & Compiling the Code
 1. git clone --recurse-submodules https://github.com/znuh/stm32-ACMconsole.git your-project
 2. cd your-project
 3. make -C libopencm3 # (Only needed once)
 4. make -C ACMconsole

If you have an older git, or got ahead of yourself and skipped the ```--recurse-submodules```
you can fix things by running ```git submodule update --init``` (This is only needed once)

# Directories
* ACMconsole contains the demo / template ttyACM application
* common-code contains USB handling, platform-specific code and the console implementation

# Memory Usage
* the **base code** for the ACM USB device makes up **~7.7 kB** (code in flash memory)
* the **console** (with history, help, tab completion, etc.) on top adds **~6.5 kB** (code in flash memory)  
=> you need a STM32 with **≥16kB** flash memory to use this
* stdio (for (f)puts, etc. - **without \*printf**) adds ~3.5 kB (optional)
=> you need a STM32 with ≥32kB flash memory to use this

* BSS usage is ~3.6 kB - can be reduced by shrinking the (1 KiB) USB ACM TX buffer
