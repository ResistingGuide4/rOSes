#ifndef THREAD_H
#define THREAD_H 1

#include <stdint.h>
#include <stdbool.h>

typedef struct  __attribute__((packed)) {
    uint8_t *esp0;
    uint8_t *esp;
    uint32_t *cr3;
} tcb_t;

void switch_thread(bool send_eoi);
tcb_t *new_thread(uint32_t *eip, uint32_t *cr3);
void kill_thread(uint32_t index);
void read_threads();
uint32_t get_thread_index();

#endif