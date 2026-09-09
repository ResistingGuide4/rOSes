#include <kernel/gdt.h>

#define GDT_MAX_DESCRIPTORS 200

extern void reloadSegments(void);
extern uint32_t stack_top;

tss_t __attribute__((section(".bootdata"), used)) tss_1;

gdt_entry_t __attribute__((section(".bootdata"), used)) gdt[GDT_MAX_DESCRIPTORS];

static gdtr_t __attribute__((section(".bootdata"), used)) gdtr;

void __attribute__((section(".boot"), used)) create_gdt_entry(uint8_t index, uint32_t base, uint32_t limit, uint16_t flags) {
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

void __attribute__((section(".boot"), used)) gdt_init(void) {
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