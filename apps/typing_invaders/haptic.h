// haptic.h - vibration motor on GPIO 46 (no BSP driver yet; plain on/off).
#ifndef HAPTIC_H
#define HAPTIC_H
#include <stdint.h>

void haptic_init(void);
void haptic_pulse(uint32_t ms);
void haptic_double(uint32_t on_ms, uint32_t gap_ms);   /* buzz-buzz */
void haptic_task(void);                                /* call every pass */
#endif
