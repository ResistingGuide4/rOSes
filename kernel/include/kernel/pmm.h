#ifndef PMM_H
#define PMM_H 1

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <kernel/multiboot2.h>

void read_mmap();
void pmm_init(struct multiboot_tag_mmap *mmap, uint32_t magic);
void read_free_blocks();
void *alloc_page();
void free_page(void *addr);
void coalesce();
bool test_pmm();

typedef struct {
    void *addr;
    size_t len; //Length in Pages
} __attribute__((packed)) pmm_block_t;
#endif