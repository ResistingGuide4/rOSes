#ifndef VMM_H
#define VMM_H

#include <stdint.h>
typedef struct {
    void *addr;
    uint32_t size; //in pages
} __attribute__((packed)) vmm_block_t;

void vmm_init();
void *alloc_block(uint32_t size);
void free_block(void *addr, uint32_t size);
void read_kernel_space();
void read_user_space();
void *get_physaddr(void *virtualaddr);

#endif