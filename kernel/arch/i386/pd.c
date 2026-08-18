#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <kernel/multiboot2.h>

extern uint32_t page_directory_start[1024];
extern uint32_t page_table0_start[1024];
extern uint32_t page_table768_start[1024];

void __attribute__((section(".boot"), used)) pd_init () {
    for (int i = 0; i < 1024; i++) {
        page_table0_start[i] = (i * 0x1000) | 0x3;
        page_table768_start[i] = (i * 0x1000) | 0x3;
    }

    page_directory_start[0] = (uint32_t)page_table0_start | 0x3;
    page_directory_start[768] = (uint32_t)page_table768_start | 0x3;
    page_directory_start[1023] = (uint32_t)page_directory_start | 0x3;
}