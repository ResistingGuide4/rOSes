#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/thread.h>

__attribute__((noreturn))
void kernel_main(void) {
	printf("Kernel Finished\n");
	
	for (;;) {
		asm volatile ("hlt");
	}
}