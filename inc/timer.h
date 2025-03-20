#ifndef INC_TIMER_H
#define INC_TIMER_H

void timer_init();
void timer_intr();

// fxl timer.c
void current_time(unsigned *sec, unsigned *msec);
unsigned long current_counter(); // in ms
#endif
