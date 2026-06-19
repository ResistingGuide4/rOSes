# Declare constants for the multiboot header.
.set FLAGS,    0  # this is the Multiboot 'flag' field
.set MAGIC,    0xE85250D6       # 'magic number' lets bootloader find the header
.set SIZE, header_end - header_start
.set CHECKSUM, -(MAGIC + FLAGS + SIZE) # checksum of above, to prove we are multiboot

# Declare a header as in the Multiboot Standard.
.section .multiboot
.align 8
header_start:
.long MAGIC
.long FLAGS
.long SIZE
.long CHECKSUM
# Tags go here

# Ending tag
.short 0
.short 0
.long 8
header_end:
# Reserve a stack for the initial thread.
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

# The kernel entry point.
.section .text
.global _start
.type _start, @function
_start:
	movl $stack_top, %esp

	# Setup necessary components like the GDT and Memory Management
	call kernel_early_main

	# Call the global constructors.
	call _init

	# Transfer control to the main kernel.
	call kernel_main

	# Hang if kernel_main unexpectedly returns.
	cli
1:	hlt
	jmp 1b
.size _start, . - _start

.global reloadSegments
reloadSegments:
   # Reload CS register containing code selector:
   ljmp  $0x08, $.reload_CS # 0x08 is a stand-in for your code segment
.reload_CS:
   # Reload data segment registers:
   movw  $0x10, %ax # 0x10 is a stand-in for your data segment
   movw  %ax, %ds
   movw  %ax, %es
   movw  %ax, %fs
   movw  %ax, %gs
   movw  %ax, %ss
   ret