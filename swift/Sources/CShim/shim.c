#include "shim.h"

uint64_t shim_rdtsc(void) {
#if defined(__aarch64__)
  uint64_t val;
  // Read the virtual counter (CNTVCT_EL0)
  __asm__ volatile("mrs %0, cntvct_el0" : "=r"(val));
  return val;
#elif defined(__x86_64__)
  uint32_t lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
#else
  return 0; // Fallback or error
#endif
}

void shim_nop(void) { __asm__ volatile("nop"); }
