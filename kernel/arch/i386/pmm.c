#include <stdio.h>

#include <kernel/pmm.h>
#include <kernel/multiboot2.h>

struct multiboot_tag_mmap *memory_map;
uint32_t multiboot_magic;

free_block_t free_blocks[1024];
uint16_t last_freed = -1;

void pmm_init(struct multiboot_tag_mmap *mmap, uint32_t magic) {
    memory_map = mmap;
    multiboot_magic = magic;

    struct multiboot_mmap_entry *entry =
            (struct multiboot_mmap_entry *)(uintptr_t)mmap->entries;
    while ((void *)entry < (void *)mmap + mmap->size) {
        if (entry->type == 1 && entry->addr_low <= 0xFFFFF000) {
            uint32_t aligned_addr = (entry->addr_low + 4095) & ~4095;  // Align up
            uint32_t end_addr = entry->addr_low + entry->len_low;
            
            if (end_addr > aligned_addr && end_addr <= 0x100000000) {
                uint32_t len = end_addr - aligned_addr;
                uint32_t pages = len / 4096;
                
                if (pages) {
                    last_freed++;
                    free_blocks[last_freed].addr = aligned_addr;
                    free_blocks[last_freed].len = pages;
                }
            }
        }
        entry = (void *)entry + mmap->entry_size;
    }
}

void readFreeBlocks() {
    for (int i = 0; i <= last_freed; i++) {
        printf("Address: %X | Size in Pages: %u\n", free_blocks[i].addr, free_blocks[i].len);
    }
    printf("Last Freed: %d\n\n", last_freed);
}

void readMMap() {
	printf("Magic: %d\n", multiboot_magic);
	struct multiboot_mmap_entry *entry =
	    (struct multiboot_mmap_entry *)(uintptr_t)memory_map->entries;
	while ((void *)entry < (void *)memory_map + memory_map->size) {
		printf("Start Addr Low: %X | Length Low: %X | Type: %d.\n",
		    (unsigned int)entry->addr_low, (unsigned int)entry->len_low, (unsigned int)entry->type);
        
		entry = (void *)entry + memory_map->entry_size;
	}
    printf("\n");
}