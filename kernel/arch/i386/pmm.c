#include <stdio.h>
#include <string.h>

#include <kernel/pmm.h>
#include <kernel/multiboot2.h>

struct multiboot_tag_mmap *memory_map;
uint32_t multiboot_magic;

#define FREE_ARRAY_SIZE 1024

pmm_block_t free_blocks[FREE_ARRAY_SIZE];
pmm_block_t *last_freed = free_blocks - 1;

pmm_block_t *coalesce_ptr = NULL;

void pmm_init(struct multiboot_tag_mmap *mmap, uint32_t magic) {
    memory_map = mmap;
    multiboot_magic = magic;

    struct multiboot_mmap_entry *entry =
            (struct multiboot_mmap_entry *)(uintptr_t)mmap->entries;
    while ((void *)entry < (void *)mmap + mmap->size) {
        if (entry->type == 1 && entry->addr_low <= 0xFFFFF000) {
            uint32_t aligned_addr = (entry->addr_low + 4095) & ~4095;  // Align uP
            if (aligned_addr < 0x400000 /*4 Mib*/) {
                aligned_addr = 0x400000; //First 4 Mib is already mapped
            }

            uint64_t end_addr = entry->addr_low + entry->len_low;
            if (end_addr > 0x100000000) {
                end_addr = 0x100000000;
            }
            
            if (end_addr > aligned_addr) {
                uint32_t len = end_addr - aligned_addr;
                uint32_t pages = len / 4096;

                if (pages) {
                    last_freed++;
                    last_freed->addr = (void *)aligned_addr;
                    last_freed->len = pages;
                }
            }
        }
        entry = (void *)entry + mmap->entry_size;
    }

    coalesce_ptr = last_freed + 1;
}

void *alloc_page() {
    if (last_freed < free_blocks) {
        //TODO: Swap out to disk
        return NULL;
    }

    if (--(last_freed->len) == 0) {
        last_freed--;
        return (last_freed+1)->addr;
    }
    return (void *)(last_freed->addr + 4096 * last_freed->len);
}

void free_page(void *addr) {
    if (addr == last_freed->addr + last_freed->len * 4096) {
        last_freed->len++;
        return;
    }
    last_freed++;
    last_freed->addr = addr;
    last_freed->len = 1;
}

static void coalesce_loop(pmm_block_t *block) {
    for (pmm_block_t *ptr = free_blocks; ptr <= last_freed; ++ptr) {
        if (ptr == block) {
            continue;
        }
        if (ptr->addr + ptr->len * 4096 == block->addr) {
            if (block < coalesce_ptr) {
                coalesce_ptr--;
            }
            ptr->len += block->len;
            memmove(block, block + 1, (size_t)(last_freed - block) * sizeof *block);
            last_freed--;
            if (ptr > block) {
                ptr--;
            }
            coalesce_loop(ptr);
            coalesce_ptr++;
            return;
        }
        if (block->addr + block->len * 4096 == ptr->addr) {
            if (block < coalesce_ptr) {
                coalesce_ptr--;
            }
            ptr->addr = block->addr;
            ptr->len += block->len;
            memmove(block, block + 1, (size_t)(last_freed - block) * sizeof *block);
            last_freed--;
            if (ptr > block) {
                ptr--;
            }
            coalesce_loop(ptr);
            coalesce_ptr++;
            return;
        }
    }
}

void coalesce() {
    if (last_freed == free_blocks || coalesce_ptr == NULL) {
        return;
    }
    while (coalesce_ptr <= last_freed) {
        coalesce_loop(coalesce_ptr);
        coalesce_ptr++;
    }
    coalesce_ptr = last_freed + 1;
}

void read_free_blocks() {
    for (int i = 0; i <= last_freed - free_blocks; i++) {
        printf("Address: %X | Size in Pages: %u\n", free_blocks[i].addr, free_blocks[i].len);
    }
    printf("Last Freed Index: %X\n", last_freed - free_blocks);
}

void read_mmap() {
	printf("Magic: %d\n", multiboot_magic);
	struct multiboot_mmap_entry *entry =
	    (struct multiboot_mmap_entry *)(uintptr_t)memory_map->entries;
	while ((void *)entry < (void *)memory_map + memory_map->size) {
		printf("Start Addr Low: %X | Length Low: %X | Type: %d.\n",
		    (unsigned int)entry->addr_low, (unsigned int)entry->len_low, (unsigned int)entry->type);
        
		entry = (void *)entry + memory_map->entry_size;
	}
}

static uint32_t checksum() {
	uint32_t key = 2983;

    pmm_block_t *ptr;
	for (size_t i = 0; i < (size_t)FREE_ARRAY_SIZE; i++) {
        ptr = free_blocks + i;
        key *= (uint32_t)(ptr->addr) + 1;
        key *= ptr->len;
    }

	return key;
}

bool test_pmm() {
    uint32_t start_key = checksum();

    uint32_t *a = alloc_page();
    uint32_t *b = alloc_page();
    uint32_t *c = alloc_page();

    free_page(a);
    free_page(b);
    free_page(c);

    coalesce();
    
    if (start_key != checksum()) {
        return false;
    }
    return true;
}