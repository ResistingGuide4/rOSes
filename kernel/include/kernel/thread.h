#ifndef THREAD_H
#define THREAD_H 1

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t *esp0;
    uint8_t *esp;
    uint32_t *cr3;
} tcb_t;

void switch_thread(bool send_eoi);
tcb_t *new_thread(uint32_t *eip, uint32_t *cr3);
void kill_thread(tcb_t *thread);
void read_threads();

#endif