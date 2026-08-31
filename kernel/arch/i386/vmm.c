#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include <kernel/vmm.h>
#include <kernel/pmm.h>

vmm_block_t user_space[512]; //Binary Tree representing free space below 0xC0000000
vmm_block_t *user_space_end = user_space - 1; //Pointer to the last free block in user_space

vmm_block_t kernel_space[512];//Binary Tree representing free space above 0xC0000000
vmm_block_t *kernel_space_end = kernel_space - 1; //Same function as user_space_end

void vmm_init() {
    user_space_end++;
    user_space_end->addr = (void *)0x400000;
    user_space_end->size = 785408;

    kernel_space_end++;
    kernel_space_end->addr = (void *)0xC0001000;
    kernel_space_end->size = 245760;
}

static vmm_block_t *check_lower(vmm_block_t *block, bool is_kernel_space) {
    //TODO: Add combining if a match is found
    vmm_block_t tmp;
    vmm_block_t *space_end = (is_kernel_space ? kernel_space_end : user_space_end) + 512;
    while (block < space_end && block->size < (block+1)->size) {
        tmp = *(block+1);
        *(block + 1) = *block;
        *block = tmp;
        block += 1;
    }
    return block;
}

static vmm_block_t *check_upper(vmm_block_t *block, bool is_kernel_space) {
    //TODO: Add combining if a match is found
    vmm_block_t tmp;
    vmm_block_t *space_start = (is_kernel_space ? kernel_space : user_space);
    while (block > space_start && block->size > (block-1)->size) {
        tmp = *(block-1);
        *(block-1) = *block;
        *block = tmp;
        block -= 1;
    }

    for (vmm_block_t *ptr = block - 1; ptr >= space_start; ptr--) {
        if (block->addr == (ptr->addr + ptr->size * 4096)) {
            ptr->size += block->size;
            memmove(block + 1, block, (block + 1 - (is_kernel_space ? kernel_space_end : user_space_end)) * sizeof(vmm_block_t));
            if (is_kernel_space) {
                kernel_space_end--;
            } else {
                user_space_end--;
            }
            return ptr;
        }
    }

    return block;
}

static void flush_tlb() {
    asm volatile (
        "movl %%cr3, %%eax\n\t"
        "movl %%eax, %%cr3"
        :
        :
        : "eax"
    );
}

void *get_physaddr(void *virtualaddr) {
    unsigned long pdindex = (unsigned long)virtualaddr >> 22;
    unsigned long ptindex = (unsigned long)virtualaddr >> 12 & 0x03FF;

    unsigned long *pd = (unsigned long *)0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    if ((pd[pdindex] & 0x1) == 0x00) {
        return NULL;
    }

    unsigned long *pt = ((unsigned long *)0xFFC00000) + (0x400 * pdindex);
    // Here you need to check whether the PT entry is present.
    if ((pt[ptindex] & 0x1) == 0x00) {
        return NULL;
    }

    return (void *)((pt[ptindex] & ~0xFFF) + ((unsigned long)virtualaddr & 0xFFF));
}

static void map_page(void *physaddr, void *virtualaddr, unsigned int flags) {
    // Make sure that both addresses are page-aligned.
    physaddr = (void *)((uintptr_t)physaddr & 0xFFFFF000);
    virtualaddr = (void *)((uintptr_t)virtualaddr & 0xFFFFF000);

    unsigned long pdindex = (unsigned long)virtualaddr >> 22;
    unsigned long ptindex = (unsigned long)virtualaddr >> 12 & 0x03FF;

    unsigned long *pd = (unsigned long *)0xFFFFF000;
    unsigned long *pt = ((unsigned long *)0xFFC00000) + (0x400 * pdindex);

    if ((pd[pdindex] & 0x01) == 0x00) {
        void *pde_page = alloc_page();
        if (pde_page != NULL) {
            pd[pdindex] = (uintptr_t)pde_page | 0x7;
            for (unsigned int i = 0; i < 1024; i++) {
                pt[i] = 0x0;
            }
        } else {
            //TODO: Add abort
        }
    }
    
    //When a mapping is already present, overwrite it and flush the TLB afterwards
    if ((pt[ptindex] & 0x01) == 0x01) {
        pt[ptindex] = ((unsigned long)physaddr) | (flags & 0xFFF) | 0x01;
        flush_tlb();
    } else {
        pt[ptindex] = ((unsigned long)physaddr) | (flags & 0xFFF) | 0x01; // Present
    }
}

static void unmap_page(void *virtualaddr) {
    // Make sure that the virt address is page-aligned
    virtualaddr = (void *)((uintptr_t)virtualaddr & 0xFFFFF000);

    unsigned long pdindex = (unsigned long)virtualaddr >> 22;
    unsigned long ptindex = (unsigned long)virtualaddr >> 12 & 0x03FF;

    unsigned long *pd = (unsigned long *)0xFFFFF000;
    
    if ((pd[pdindex] & 0x01) == 0x00) {
        return;
    }

    unsigned long *pt = ((unsigned long *)0xFFC00000) + (0x400 * pdindex);
    
    if ((pt[ptindex] & 0x01) == 0x00) {
        return;
    }
    pt[ptindex] = 0x00;

    //Note: This function does not flush the TLB to save time as this only unmaps 1 page. Anything that calls this should flush it after all of it's calls to this function.
}

void *alloc_block(uint32_t size) {
    void *virt_addr = NULL;
    for (vmm_block_t *block = user_space_end; block >= user_space; block--) {
        if (block->size >= size) {
            block->size -= size;
            block = check_lower(block, false);
            virt_addr = block->addr + block->size * 4096;
            break;
        }
    }

    if (virt_addr == NULL)   {
        //TODO: Run defragmentation function and return NULL if no changes are made;
        return NULL;
    }
    for (unsigned int i = 0; i < size; i++) {
        void *phys_addr = alloc_page();
        if (phys_addr == NULL) {
            return NULL;
        }
        map_page(phys_addr, virt_addr + i * 0x1000, 0x02);
    }

    return virt_addr;
}

void free_block(void *addr, uint32_t size) {
    if (++user_space_end == user_space + 512) {
        //TODO: RUN DEFRAGMENT FUNCTION AND FAIL IF NO CHANGES ARE MADE
        return;
    }

    user_space_end->addr = addr;
    user_space_end->size = size;

    check_upper(user_space_end, false);

    for (unsigned int i = 0; i < size; i++) {
        unmap_page(addr + i * 4096);
    }
    flush_tlb();
}

void read_kernel_space() {
    for (vmm_block_t *ptr = kernel_space; ptr <= kernel_space_end; ptr++) {
        printf("Address: %X | Size: %u\n", ptr->addr, ptr->size);
    }
    printf("\n");
}

void read_user_space() {
    for (vmm_block_t *ptr = user_space; ptr <= user_space_end; ptr++) {
        printf("Address: %X | Size: %u\n", ptr->addr, ptr->size);
    }
    printf("\n");
}