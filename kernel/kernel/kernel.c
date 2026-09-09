#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/heap.h>

void kernel_main(void) {
	terminal_initialize();
	uint32_t *a = (uint32_t *)0xD0000000;
	*a = 12345;
	printf("%d", *a);
}