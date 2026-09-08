#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include <kernel/multiboot2.h>
#include <kernel/heap.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>

heap_header_t *start_header;
heap_header_t *end_header;

struct multiboot_tag *multiboot_find_tag(void *mbd, uint32_t type) {
    /* The multiboot info structure begins with a 32-bit integer
    *  indicating the total size of the structure. This is then
    *  followed by a 32-bit reserved region and then by the tags. */
	struct multiboot_tag *tag = (void *)mbd + 8;
	while ((void *)tag < (void *)mbd + *((uint32_t *)mbd)) {
		if (tag->type == type) {
			return tag;
		}
		tag = (void *)tag + tag->size;
		/* Tags are always aligned on 8-byte boundaries. */
		if ((uintptr_t)tag % 8 > 0) {
			tag = (void *)tag + 8 - ((uintptr_t)tag % 8);
		}
	}
	return 0;
}

void heap_init(void *mbd, uint32_t magic) {
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        abort();
    }
    
    struct multiboot_tag_mmap *tag_mmap =
	    (struct multiboot_tag_mmap *)multiboot_find_tag(
		mbd, MULTIBOOT_TAG_TYPE_MMAP);

	if (!tag_mmap) {
		abort();
	}

	pmm_init(tag_mmap, magic);
	vmm_init();

    start_header = alloc_block(16, true);
    if (start_header == NULL) {
		abort();
    }

	start_header->status = HEAP_FREE;
	start_header->size = 0x10000 - sizeof(heap_header_t); //Size of 16 4kib pages minus the header
	start_header->next = NULL;
	start_header->prev = NULL;
}

uint32_t flags;

static void kmalloc_lock() {
	asm volatile (
		"pushfl\n\t"
		"pop %0\n\t"
		"cli"
		: "=rm" (flags)
		:
		: "memory"
	);
}

static void kmalloc_unlock() {
	if (flags & (0x1 << 9)) {
		asm volatile ("sti" ::: "memory");
	}
}

static heap_header_t *split_header(heap_header_t *header, size_t size) {//Takes a chunk of a header and marks it as used
	header->size -= size + sizeof(heap_header_t);
	heap_header_t *next_header = (heap_header_t *)((uintptr_t)header + sizeof(heap_header_t) + header->size);
	next_header->size = size;
	next_header->status = HEAP_USED;
	
	next_header->prev = header;
	next_header->next = header->next;
	if (next_header->next != NULL) {
		next_header->next->prev = next_header;
	}
	header->next = next_header;

	kmalloc_unlock();
	return next_header;
}
void *kmalloc(size_t size) {
	kmalloc_lock();

	size_t aligned_size = (size + 7) & ~7; //Keeps return address and header sizes 8-byte aligned

	heap_header_t *current_header = start_header;

	while (current_header != NULL) {
		if (current_header->status == HEAP_FREE) {
			if (current_header->size == aligned_size) {
				current_header->status = HEAP_USED;
				kmalloc_unlock();
				return current_header + 1; //Address directly after the header
			}
			if (current_header->size >= aligned_size + sizeof(heap_header_t)) {
				return split_header(current_header, aligned_size) + 1;
			}
		}

		if (current_header->next == NULL) {
			break;
		}

		current_header = current_header->next;
	}

	//No block big enough found
	current_header->next = alloc_block((aligned_size + sizeof(heap_header_t) + 4095) / 4096, true);
	if (current_header->next) {
		current_header->next->prev = current_header;
		current_header = current_header->next;
		current_header->size = ((aligned_size + sizeof(heap_header_t) + 4095) & ~4095) - sizeof(heap_header_t);
		current_header->status = HEAP_FREE;
		
		return split_header(current_header, aligned_size) + 1;
	} else {
		kmalloc_unlock();
		return NULL;
	}
}

void kfree(void *addr) {
	heap_header_t *current_header = addr - sizeof(heap_header_t);

	if (current_header->status != HEAP_FREE && current_header->status != HEAP_USED) {
		return;
	}

	current_header->status = HEAP_FREE;
	
	heap_header_t *prev_header = current_header->prev;
	while (prev_header != NULL && prev_header->status == HEAP_FREE && (uint8_t *)prev_header + sizeof(heap_header_t) + prev_header->size == current_header) {
		prev_header->size += sizeof(heap_header_t) + current_header->size;
		prev_header->next = current_header->next;
		current_header = prev_header;
		prev_header = current_header->prev;
	}

	heap_header_t *next_header = current_header->next;
	while (next_header != NULL && next_header->status == HEAP_FREE && (uint8_t *)current_header + sizeof(heap_header_t) + current_header->size == next_header) {
		current_header->size += sizeof(heap_header_t) + next_header->size;
		current_header->next = next_header->next;
		next_header = current_header->next;
	}
}

void read_heap() {
	for (heap_header_t *ptr = start_header; ptr != NULL; ptr = ptr->next) {
		printf("Addr of Block: %X | Size: %X | Status: %s\n", (uintptr_t)(ptr + 1), ptr->size, (ptr->status == HEAP_FREE ? "FREE" : "USED"));
	}
	printf("\n");
}