#ifndef PMM_H
#define PMM_H 1

#include <stdint.h>
#include <stddef.h>

#include <kernel/multiboot2.h>

void readMMap();
void pmm_init(struct multiboot_tag_mmap *mmap, uint32_t magic);
void readFreeBlocks();
void *alloc_page();
void free_page(uint32_t addr);

typedef struct {
    uint32_t addr;
    size_t len; //Length in Pages
} __attribute__((packed)) pmm_block_t;
#endif