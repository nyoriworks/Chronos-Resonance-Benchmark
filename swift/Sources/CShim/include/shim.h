#ifndef shim_h
#define shim_h

#include <stdint.h>

uint64_t shim_rdtsc(void);
void shim_nop(void);

#endif /* shim_h */
