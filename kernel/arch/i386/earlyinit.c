#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

extern void reloadSegments(void);
extern uint32_t stack_top;

typedef struct __attribute__((packed)) {
    uint32_t link;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldtr;
    uint16_t reserved; // To move iobp 2 bytes forward
    uint16_t iopb;
    uint32_t ssp;
} tss_t;

tss_t tss_1;

// Each define here is for a specific flag in the descriptor.
// Refer to the intel documentation for a description of what each one does.
#define SEG_DESCTYPE(x)  ((x) << 0x04) // Descriptor type (0 for system, 1 for code/data)
#define SEG_PRIV(x)     (((x) &  0x03) << 0x05)   // Set privilege level (0 - 3)
#define SEG_PRES(x)      ((x) << 0x07) // Present
#define SEG_SAVL(x)      ((x) << 0x0C) // Available for system use
#define SEG_LONG(x)      ((x) << 0x0D) // Long mode
#define SEG_SIZE(x)      ((x) << 0x0E) // Size (0 for 16-bit, 1 for 32)
#define SEG_GRAN(x)      ((x) << 0x0F) // Granularity (0 for 1B - 1MB, 1 for 4KB - 4GB)
 
#define SEG_DATA_RD        0x00 // Read-Only
#define SEG_DATA_RDA       0x01 // Read-Only, accessed
#define SEG_DATA_RDWR      0x02 // Read/Write
#define SEG_DATA_RDWRA     0x03 // Read/Write, accessed
#define SEG_DATA_RDEXPD    0x04 // Read-Only, expand-down
#define SEG_DATA_RDEXPDA   0x05 // Read-Only, expand-down, accessed
#define SEG_DATA_RDWREXPD  0x06 // Read/Write, expand-down
#define SEG_DATA_RDWREXPDA 0x07 // Read/Write, expand-down, accessed
#define SEG_CODE_EX        0x08 // Execute-Only
#define SEG_CODE_EXA       0x09 // Execute-Only, accessed
#define SEG_CODE_EXRD      0x0A // Execute/Read
#define SEG_CODE_EXRDA     0x0B // Execute/Read, accessed
#define SEG_CODE_EXC       0x0C // Execute-Only, conforming
#define SEG_CODE_EXCA      0x0D // Execute-Only, conforming, accessed
#define SEG_CODE_EXRDC     0x0E // Execute/Read, conforming
#define SEG_CODE_EXRDCA    0x0F // Execute/Read, conforming, accessed
 
#define GDT_CODE_PL0 SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | \
                     SEG_LONG(0)     | SEG_SIZE(1) | SEG_GRAN(1) | \
                     SEG_PRIV(0)     | SEG_CODE_EXRD
 
#define GDT_DATA_PL0 SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | \
                     SEG_LONG(0)     | SEG_SIZE(1) | SEG_GRAN(1) | \
                     SEG_PRIV(0)     | SEG_DATA_RDWR
 
#define GDT_CODE_PL3 SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | \
                     SEG_LONG(0)     | SEG_SIZE(1) | SEG_GRAN(1) | \
                     SEG_PRIV(3)     | SEG_CODE_EXRD
 
#define GDT_DATA_PL3 SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | \
                     SEG_LONG(0)     | SEG_SIZE(1) | SEG_GRAN(1) | \
                     SEG_PRIV(3)     | SEG_DATA_RDWR

typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t limit_high : 4;
    uint8_t flags : 4;
    uint8_t base_high;
} gdt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} gdtr_t;

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

void kernel_early_main(void) {

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