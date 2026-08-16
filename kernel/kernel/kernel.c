#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/testing.h>

void kernel_main(void) {
	terminal_initialize();
	printf("Hello, kernel World!%x\n", 4294967295);
	readMMap();
}
