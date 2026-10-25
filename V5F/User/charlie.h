#ifndef CHARLIE_H
#define CHARLIE_H
#include <stdint.h>
#define LEDON  1
#define LEDOFF 0
static inline void LED_SetPixel(int idx, int on) { (void)idx; (void)on; }
#endif
