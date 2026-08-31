#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>

void kernel_main(void) {
	terminal_initialize();
	uint32_t *alloced = alloc_block(16);
	read_user_space();
	free_block(alloced, 16);
	read_user_space();
}