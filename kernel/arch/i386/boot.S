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
.irp vector, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33
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

   # Initialize Heap and Memory Managment
   call heap_init

   # Add the current process to the list of threads
   call thread_init

   pushl $page_directory_start
   pushl $background_thread
   call new_thread
   pushl 0x1
   call switch_thread
   popl %eax
   popl %eax

   # Initialize the Stack-Smashing Protector
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

background_thread:
hlt

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
isr_no_err_stub 32 //Start of PIC Interrupts
isr_stub_33:
   call keyboard_handler
   iret

.extern current_task_TCB
.extern piti
.global switch_tasks
switch_tasks:
   # Save previous task's state

   # Notes:
   #  For cdecl, EAX, ECX, and EDX are already saved by the caller and don't need to be saved again
   #  EIP is already saved on the stack by the caller's "CALL" instruction
   #  The task isn't able to change CR3 so it doesn't need to be saved
   #  Segment registers are constants (while running kernel code) so they don't need to be saved

   pushl %ebx
   pushl %esi
   pushl %edi
   pushl %ebp

   movl current_task_TCB, %edi  # edi = address of the previous task's thread control block
   movl %esp, 4(%edi)           # Update esp on the previous task's TCB

   # Load next task's state
   movl 20(%esp), %esi          # esi = next task's thread control block argument
   movl %esi, current_task_TCB  # Current task's TCB is the next task TCB

   movl 4(%esi),%esp            # Load ESP for next task's kernel stack from the thread's TCB
   movl 8(%esi),%eax            # eax = address of page directory for next task
   movl (%esi),%ebx             # ebx = address for the top of the next task's kernel stack
   movl %ebx,tss_1+4            # Adjust the ESP0 field in the TSS (used by CPU for for CPL=3 -> CPL=0 privilege level changes)
   movl %cr3, %ecx              # ecx = previous task's virtual address space

   cmpl %ecx, %eax              # Does the virtual address space need to be changed?
   je .doneVAS                  # no, virtual address space is the same, so don't reload it and cause TLB flushes
   movl %eax, %cr3              # yes, load the next task's virtual address space
.doneVAS:
   movb piti, %al
   cmpb $0x1, %al
   jne .exit
   movb $0x20, %al
   outb %al, $0x20
   movb $0x0, piti
.exit:
   popl %ebp
   popl %edi
   popl %esi
   popl %ebx

   ret                           # Load next task's EIP from its kernel stack