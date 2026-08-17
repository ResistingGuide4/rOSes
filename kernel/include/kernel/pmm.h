#include <stdint.h>
#include <stddef.h>

#include <kernel/multiboot2.h>

void readMMap();
void pmm_init(struct multiboot_tag_mmap *mmap, uint32_t magic);
void readFreeBlocks();

typedef struct {
    uint32_t addr;
    size_t len; //Length in Pages
} __attribute__((packed)) free_block_t;