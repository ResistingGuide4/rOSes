#include <stdint.h>
#include <stdlib.h>

// Allocate the global guard variable
uintptr_t __stack_chk_guard = 0;

// Simple hardware-based seeding using the timestamp counter
static inline uint64_t rdtsc(void) {
    uint32_t low, high;
    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

// Called directly from your assembly bootstrap (e.g., boot.s)
void init_ssp(void) {
    uint64_t seed = rdtsc();
    
    // Simple LCG to mix up the bits
    seed = seed * 6364136223846793005ULL + 1;
    
    // Terminate with a null byte to prevent string function exploits
    #if UINT_32_MAX == UINT_PTR_MAX
        __stack_chk_guard = (seed & 0xFFFFFF00UL);
    #else
        __stack_chk_guard = (seed & 0xFFFFFFFFFFFFFF00ULL);
    #endif
}

__attribute__((noreturn))
void __stack_chk_fail(void) {
	#if __STDC_HOSTED__
		abort();
	#elif __is_myos_kernel
		panic("Stack smashing detected");
	#endif
}