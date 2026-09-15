#include <kernel/thread.h>
#include <kernel/vmm.h>

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define TCB_MAX_THREADS 6

extern void switch_tasks(tcb_t *next_task);
extern uint8_t stack_top;

tcb_t *current_task_TCB = NULL;

uint8_t used_threads = 0;

tcb_t threads[TCB_MAX_THREADS];
bool piti = false;

void thread_init() {
    threads[used_threads].esp0 = &stack_top;
    asm volatile ("movl %%cr3, %0" : "=rm"(threads[used_threads].cr3) : : "memory");
    current_task_TCB = threads + used_threads;
    used_threads++;
}

void switch_thread(bool send_eoi) {
    piti = send_eoi | piti;
    switch_tasks((current_task_TCB == threads + used_threads - 1 ? threads : current_task_TCB + 1));
}

//Creates a new thread with its own stack. Pass NULL for CR3 if you want to create a new PD
tcb_t *new_thread(uint32_t *eip, uint32_t *cr3) {
    if (used_threads == TCB_MAX_THREADS || cr3 == NULL) {
        return NULL;
    }
    uint32_t *pd = cr3;

    uint8_t *stack = alloc_block(4, true);

    if (stack == NULL) {
        return NULL;
    }

    uint8_t *current_esp;
    
    asm volatile("movl %%esp, %0" : "=rm"(current_esp) : : "memory");

    threads[used_threads].cr3 = pd;
    threads[used_threads].esp0 = stack+0x3fff;
    threads[used_threads].esp = stack + 0x3feb; // ESP after pushing to set up for a switch
    asm volatile (
        "movl %0, %%esp\n\t"
        "pushl %1\n\t"
        "pushl 0\n\t"
        "pushl 0\n\t"
        "pushl 0\n\t"
        "pushl %0\n\t"
        "movl %2, %%esp"
        :
        : "rm"(stack+0x3fff), "rm"(eip), "rm"(current_esp)
        : "memory"
    );

    if (current_task_TCB == NULL) {
        current_task_TCB = threads + used_threads;
    }

    return threads + (used_threads++);
}

// Make sure to switch to a different thread before killing the thread
void kill_thread(tcb_t *thread) {
    if (thread < threads + 6 && thread >= threads) {
        memmove(thread, thread + 1, sizeof(tcb_t) * (TCB_MAX_THREADS - (thread - threads) - 1));
        if (current_task_TCB >= thread) {
            current_task_TCB--;
        }
        used_threads--;
        free_block(thread->esp0-0x3fff, 4, true);
    }
}

void read_threads() {
    for (unsigned int i = 0; i < used_threads; i++) {
        printf("Thread esp0: %X | Thread esp: %X | Thread cr3: %X\n", threads[i].esp0, threads[i].esp, threads[i].cr3);
    }
    printf("%d", used_threads);
    printf("\n");
}