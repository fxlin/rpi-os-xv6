// fxl: only generic timer, no systimer? 

#include "timer.h"

#include "arm.h"
#include "base.h"
#include "irq.h"
#include "console.h"
#include "proc.h"

/* Core Timer */
#define CORE_TIMER_CTRL(i)      (LOCAL_BASE + 0x40 + 4*(i))
#define CORE_TIMER_ENABLE       (1 << 1)        /* CNTPNSIRQ */

static uint64_t dt;
static uint64_t cnt;

void
timer_init()
{
    dt = timerfreq();
    asm volatile ("msr cntp_ctl_el0, %[x]"::[x] "r"(1));
    asm volatile ("msr cntp_tval_el0, %[x]"::[x] "r"(dt));
    put32(CORE_TIMER_CTRL(cpuid()), CORE_TIMER_ENABLE);
#ifdef USE_GIC
    irq_enable(IRQ_LOCAL_CNTPNS);
    irq_register(IRQ_LOCAL_CNTPNS, timer_intr);
#endif
}

static void
timer_reset()
{
    asm volatile ("msr cntp_tval_el0, %[x]"::[x] "r"(dt));
}

/*
 * This is a per-cpu non-stable version of clock, frequency of 
 * which is determined by cpu clock (may be tuned for power saving).
 */
void
timer_intr()
{
    trace("t: %d", ++cnt);
    timer_reset();
    yield();
}

// ------------ fxl, systimer for timekeeping (mar 2025) --------------
// mapping: cf memlayout.h

#define PBASE           (0x3F000000+KERNBASE)      // start of peripheral addr, va

#define TIMER_CS        (PBASE+0x00003000)
#define TIMER_CLO       (PBASE+0x00003004)      // counter low 32bit, ro
#define TIMER_CHI       (PBASE+0x00003008)      // counter high 32bit, ro 

#define CLOCKHZ	1000000	// rpi3 use 1MHz clock for system counter. 

#define TICKPERSEC (CLOCKHZ)
#define TICKPERMS (CLOCKHZ / 1000)
#define TICKPERUS (CLOCKHZ / 1000 / 1000)

// return # of ticks (=us when clock is 1MHz)
// NB: use current_time() below to get converted time
unsigned long current_counter() {
	// assume these two are consistent, since the clock is only 1MHz...
	return ((unsigned long) get32(TIMER_CHI) << 32) | get32(TIMER_CLO); 
}

void current_time(unsigned *sec, unsigned *msec) {
	unsigned long cur = current_counter();
	*sec =  (unsigned) (cur / TICKPERSEC); 
	cur -= (*sec) * TICKPERSEC; 
	*msec = (unsigned) (cur / TICKPERMS);	
}