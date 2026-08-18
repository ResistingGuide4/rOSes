#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/pmm.h>

void kernel_main(void) {
	terminal_initialize();
	readFreeBlocks();
	readMMap();
}
