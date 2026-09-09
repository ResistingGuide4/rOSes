#include <kernel/idt.h>
#include <kernel/isr.h>

#include <stdbool.h>

#define IDT_MAX_DESCRIPTORS 256

static bool vectors[IDT_MAX_DESCRIPTORS];
extern void* isr_stub_table[];

idtr_t idtr;
idt_entry_t __attribute__((aligned(0x10))) idt[IDT_MAX_DESCRIPTORS];

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->offset_low = (uint32_t)isr & 0xFFFF;
    descriptor->kernel_cs = 0x08; // this value can be whatever offset your kernel code selector is in your GDT
    descriptor->flags = flags;
    descriptor->offset_high = (uint32_t)isr >> 16;
    descriptor->reserved = 0;
}

void idt_init() {
    __asm__ volatile ("cli");

    PIC_remap(0x20, 0x28);
    
    for (int i = 0; i < 16; i++) {
        IRQ_set_mask(i);
    }

    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * 256 - 1;

    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
        vectors[vector] = true;
    }

    __asm__ volatile ("lidt %0" : : "m"(idtr)); // load the new IDT
}