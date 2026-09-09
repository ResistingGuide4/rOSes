#ifndef IDT_H
#define IDT_H 1

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} idtr_t;

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t kernel_cs;
    uint8_t reserved;
    uint8_t flags;
    uint16_t offset_high;
} idt_entry_t;

#endif