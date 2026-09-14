#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/thread.h>

__attribute__((noreturn))
void kernel_main(void) {
	terminal_initialize();
	read_threads();
	
	for (;;) {
		asm volatile ("hlt");
	}
}