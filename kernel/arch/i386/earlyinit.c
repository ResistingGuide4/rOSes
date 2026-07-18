#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <kernel/gdt.h>
#include <kernel/multiboot2.h>

extern void reloadSegments(void);
extern uint32_t stack_top;

tss_t tss_1;

gdt_entry_t gdt[200];

gdtr_t gdtr;

void create_gdt_entry(uint8_t index, uint32_t base, uint32_t limit, uint16_t flags) {
    if (limit > 0xFFFFF) {
        return;
    }
    gdt_entry_t *ret_entry = &gdt[index];

    ret_entry->limit_low = limit & 0xFFFF;
    ret_entry->base_low = base & 0xFFFF;
    ret_entry->base_mid = (base >> 0x10) & 0xFF;
    ret_entry->access = flags & 0xFF;
    ret_entry->limit_high = (limit >> 0x10) & 0xF;
    ret_entry->flags = (flags >> 0xC) & 0xF;
    ret_entry->base_high = (base >> 0x18) & 0xFF;
}

void gdt_init(void) {
    // Initialize the GDT
    create_gdt_entry(0, 0, 0, 0);
    create_gdt_entry(1, 0, 0xFFFFF, (GDT_CODE_PL0));
    create_gdt_entry(2, 0, 0xFFFFF, (GDT_DATA_PL0));
    create_gdt_entry(3, 0, 0xFFFFF, (GDT_CODE_PL3));
    create_gdt_entry(4, 0, 0xFFFFF, (GDT_DATA_PL3));
    create_gdt_entry(5, (uint32_t)&tss_1, sizeof(tss_t) - 1, SEG_CODE_EXA | SEG_PRES(1));

    tss_1.ss0 = 0x10;
    tss_1.esp = stack_top;
    tss_1.iopb = sizeof(tss_t);

    gdtr.limit = (sizeof gdt) - 1;
    gdtr.base = (uint32_t)&gdt[0];

    asm volatile ("lgdt %0" : : "m"(gdtr) : "memory");
    reloadSegments();

    asm volatile (
        "movw $0x28, %%ax\n\t\
        ltr %%ax" : : : "eax"
    );
}

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

void kernel_early_main(void *mbd, uint32_t magic) {
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        panic("invalid magic number!");
    }
    
    struct multiboot_tag_mmap *tag_mmap =
	    (struct multiboot_tag_mmap *)multiboot_find_tag(
		mbd, MULTIBOOT_TAG_TYPE_MMAP);

	if (!tag_mmap) {
		panic("No memory map tag found!\n");
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