#include <stdio.h>

#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/heap.h>

__attribute__((noreturn))
void kernel_main(void) {
	/*uint8_t *a = (uint8_t *)0x40000000;
	*a = 5;*/
	
	printf("%d\n", (uint32_t)test_pmm());
	printf("%d\n", (uint32_t)test_vmm());
	printf("%d\n", (uint32_t)test_heap());

	for (;;) {
		asm volatile ("hlt");
	}
}