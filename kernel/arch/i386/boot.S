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
.global page_table0_start
page_table0_start:
.skip 4096
.global page_table768_start
page_table768_start:
.skip 4096

.align 16
stack_bottom:
.skip 16384 # 16 KiB
.global stack_top
stack_top:

.section .data
.global isr_stub_table
isr_stub_table:
.irp vector, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
   .long isr_stub_\vector
.endr

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
   # Initialize the IDT
   call idt_init

   #Initialize Heap and Memory Managment
   call heap_init

   #Initialize the Stack-Smashing Protector
   call init_ssp

	# Call the global constructors.
	call _init

	# Transfer control to the main kernel.
	call kernel_main

	# Hang if kernel_main unexpectedly returns.
	cli
1:	hlt
	jmp 1b
.size higher_half_start, . - higher_half_start

# Create ISRs
.macro isr_err_stub vector
isr_stub_\vector:
   call exception_handler
   iret
.endm

.macro isr_no_err_stub vector
isr_stub_\vector:
   call exception_handler
   iret
.endm

.extern exception_handler
isr_no_err_stub 0
isr_no_err_stub 1
isr_no_err_stub 2
isr_no_err_stub 3
isr_no_err_stub 4
isr_no_err_stub 5
isr_no_err_stub 6
isr_no_err_stub 7
isr_err_stub    8
isr_no_err_stub 9
isr_err_stub    10
isr_err_stub    11
isr_err_stub    12
isr_err_stub    13
isr_err_stub    14
isr_no_err_stub 15
isr_no_err_stub 16
isr_err_stub    17
isr_no_err_stub 18
isr_no_err_stub 19
isr_no_err_stub 20
isr_no_err_stub 21
isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28
isr_no_err_stub 29
isr_err_stub    30
isr_no_err_stub 31