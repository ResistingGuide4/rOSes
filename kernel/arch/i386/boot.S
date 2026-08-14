# Declare constants for the multiboot 2 header.
.set FLAGS,    0  # this is the Multiboot 'flag' field
.set MAGIC,    0xE85250D6       # 'magic number' lets bootloader find the header
.set SIZE, header_end - header_start
.set CHECKSUM, -(MAGIC + FLAGS + SIZE) # checksum of above, to prove we are multiboot

# Declare a header as in the Multiboot Standard.
.section .multiboot,"a",@progbits
.align 8
header_start:
.long MAGIC
.long FLAGS
.long SIZE
.long CHECKSUM
# Tags go here
.align 8
.short 1
.short 0
.long 12
.long 6
# Ending tag
.align 8
.short 0
.short 0
.long 8
header_end:

.section .bootdata
.align 4096
.global page_directory_start
page_directory_start:
.skip 4096
.global page_table1_start
page_table1_start:
.skip 4096
.global page_table768_start
page_table768_start:
.skip 4096

.align 16
stack_bottom:
.skip 16384 # 16 KiB
.global stack_top
stack_top:

# The kernel entry point.
.section .boot,"ax",@progbits
.global _start
.type _start, @function
_start:
   # Initialize Stack
	movl $stack_top, %esp

   # Push Data Requested from Bootloader to Stack
   pushl %eax
   pushl %ebx

   # Initialize the GDT
   call gdt_init

	# Initialize the Page Directory and Page Tables with Higher-Half
	call pd_init

   movl $page_directory_start, %eax
   movl %eax, %cr3

   movl %cr0, %eax
   orl $0x80000001, %eax
   movl %eax, %cr0

   ljmp $0x08, $higher_half_start
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

.section .text
higher_half_start:
   #Initialize Heap and Memory Managment
   call heap_init

	# Call the global constructors.
	call _init

	# Transfer control to the main kernel.
	call kernel_main

	# Hang if kernel_main unexpectedly returns.
	cli
1:	hlt
	jmp 1b
.size higher_half_start, . - higher_half_start