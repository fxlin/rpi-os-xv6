#include "clock.h"

#include "arm.h"
#include "peripherals/irq.h"

#include "console.h"
#include "debug.h"

void
clock_init()
{
    put32(TIMER_CTRL, TIMER_INTENA | TIMER_ENABLE | TIMER_RELOAD_SEC);
    put32(TIMER_ROUTE, TIMER_IRQ2CORE(0));
    put32(TIMER_CLR, TIMER_RELOAD | TIMER_CLR_INT);
}

void
clock_reset()
{
    put32(TIMER_CLR, TIMER_CLR_INT);
}

/*
 * clock() is called straight from a real time clock (local timer)
 * interrupt. It guaranteed to get impluse from crystal clock,
 * thus independent of the variant cpu clock.
 *
 * Functions:
 * - implement callouts
 * - maintain user/system times
 * - maintain date
 * - profile
 * - lightning bolt wakeup (every second)
 * - alarm clock signals
 */
void
clock()
{
    // debug_mem(LOCAL_BASE, 0x100);
    cprintf("cpu %d clock.\n", cpuid());
}
