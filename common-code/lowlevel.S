.syntax unified
.thumb
.section .data

/* WARNING: all cycle-based timings are for a Cortex-M0 with code execution from SRAM.
 * Other Cortexes (e.g. M0+) have different instruction timings!
 * Execution from Flash instead of SRAM can introduce additional delays
 * due to flash read waitstates.
 */  

.global delay_loop
.func delay_loop  /* only used for debugger */
.type delay_loop, STT_FUNC

// bcs: delay=0 -> 0 iterations, delay=1 -> 1 iteration , delay=2 -> 2 iterations, ...
// bhi: delay=0 -> 0 iterations, delay=1 -> 0 iterations, delay=2 -> 1 iteration, ...

delay_loop:
	subs r0,#1           /* 1 cycle  */
	bcs delay_loop       /* 3 if taken, 1 if not taken */
    bx lr                /* 3 cycles */
.endfunc
