#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/heap.h>

__attribute__((noreturn))
void kernel_main(void) {
	terminal_initialize();

	for (;;) {
		asm volatile ("hlt");
	}
}