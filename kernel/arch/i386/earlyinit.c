#include <stdint.h>
#include <stdlib.h>

#include <kernel/multiboot2.h>

struct multiboot_tag *multiboot_find_tag(void *mbd, uint32_t type) {
    /* The multiboot info structure begins with a 32-bit integer
    *  indicating the total size of the structure. This is then
    *  followed by a 32-bit reserved region and then by the tags. */
	struct multiboot_tag *tag = (void *)mbd + 8;
	while ((void *)tag < (void *)mbd + *((uint32_t *)mbd)) {
		if (tag->type == type) {
			return tag;
		}
		tag = (void *)tag + tag->size;
		/* Tags are always aligned on 8-byte boundaries. */
		if ((uintptr_t)tag % 8 > 0) {
			tag = (void *)tag + 8 - ((uintptr_t)tag % 8);
		}
	}
	return 0;
}

void heap_init(void *mbd, uint32_t magic) {
    asm volatile("testing:");
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        abort();
    }
    
    struct multiboot_tag_mmap *tag_mmap =
	    (struct multiboot_tag_mmap *)multiboot_find_tag(
		mbd, MULTIBOOT_TAG_TYPE_MMAP);

	if (!tag_mmap) {
		abort();
	}

	struct multiboot_mmap_entry *entry =
	    (struct multiboot_mmap_entry *)(uintptr_t)tag_mmap->entries;
	while ((void *)entry < (void *)tag_mmap + tag_mmap->size) {
		/*kprintf("Start Addr: %x | Length: %x | Type: %i.\n",
		    entry->addr, entry->len, entry->type);*/
        
		entry = (void *)entry + tag_mmap->entry_size;
	}
}

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