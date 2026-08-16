#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/testing.h>

void kernel_main(void) {
	terminal_initialize();
	printf("Hello, kernel World!%d\n", 0);
	readMMap();
}
