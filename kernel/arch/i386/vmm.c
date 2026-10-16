#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>


#include <kernel/vmm.h>
#include <kernel/pmm.h>

#define SPACE_SIZE 512

vmm_block_t user_space[SPACE_SIZE]; //Binary Tree representing free space below 0xC0000000
vmm_block_t *user_space_end = user_space - 1; //Pointer to the last free block in user_space

vmm_block_t kernel_space[SPACE_SIZE];//Binary Tree representing free space above 0xC0000000
vmm_block_t *kernel_space_end = kernel_space - 1; //Same function as user_space_end

void vmm_init() {
    user_space_end++;
    user_space_end->addr = (void *)0x400000;
    user_space_end->size = 785408;

    kernel_space_end++;
    kernel_space_end->addr = (void *)0xC0400000; // Be sure to account for the recursive map at the end
    kernel_space_end->size = 260096;
}

static vmm_block_t *check_lower(vmm_block_t *block, bool is_kernel_space) {
    vmm_block_t tmp;
    vmm_block_t *space_end = (is_kernel_space ? kernel_space_end : user_space_end);
    while (block < space_end && block->size < (block+1)->size) {
        tmp = *(block+1);
        *(block + 1) = *block;
        *block = tmp;
        block += 1;
    }
    
    return block;
}

static vmm_block_t *check_upper(vmm_block_t *block, bool is_kernel_space) {
    vmm_block_t tmp;
    vmm_block_t *space_start = (is_kernel_space ? kernel_space : user_space);
    vmm_block_t *space_end = (is_kernel_space ? kernel_space_end : user_space_end);
    if (block < space_start || block > space_end) {
        return block;
    }

    while (block > space_start && block->size > (block-1)->size) {
        tmp = *(block-1);
        *(block-1) = *block;
        *block = tmp;
        block -= 1;
    }

    for (size_t i = 0; i <= (size_t)(space_end - space_start); i++) {
        vmm_block_t *ptr = &space_start[i];
        if (ptr == block) {
            continue;
        }
        if (ptr->addr == (block->addr + block->size * 4096)) {
            ptr->addr = block->addr;
            ptr->size += block->size;
            memmove(block, block + 1, (size_t)(space_end - block) * sizeof *block);
            if (ptr > block) {
                ptr--;
            }
            if (is_kernel_space) {
                kernel_space_end--;
            } else {
                user_space_end--;
            }
            check_upper(ptr, is_kernel_space);
            return ptr;
        }
        if (block->addr == (ptr->addr + ptr->size * 4096)) {
            ptr->size += block->size;
            memmove(block, block + 1, (size_t)(space_end - block) * sizeof *block);
            if (ptr > block) {
                ptr--;
            }
            if (is_kernel_space) {
                kernel_space_end--;
            } else {
                user_space_end--;
            }
            check_upper(ptr, is_kernel_space);
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

static bool map_page(void *physaddr, void *virtualaddr, unsigned int flags) {
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
            return false;
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
    return true;
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

    //Note: This function does not flush the TLB, as this only unmaps 1 page to save time. Anything that calls this should flush it after all of it's calls to this function.
}

void *alloc_block(uint32_t size, bool isKernel) {
    vmm_block_t *space = (isKernel ? kernel_space : user_space);
    vmm_block_t *space_end = (isKernel ? kernel_space_end : user_space_end);

    void *virt_addr = NULL;
    vmm_block_t *free_block = NULL;
    for (vmm_block_t *block = space_end; block >= space; block--) {
        if (block->size >= size) {
            block->size -= size;
            block = check_lower(block, isKernel);
            free_block = block;
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
        if (phys_addr == NULL || !map_page(phys_addr, virt_addr + i * 0x1000, 0x02 | (isKernel ? 0x2 : 0x6))) {
            if (phys_addr != NULL) {
                free_page(phys_addr);
            }
            free_block->size += size;
            free_block = check_upper(free_block, isKernel);
            for (unsigned int j = 0; j < i; j++) {
                free_page(get_physaddr(virt_addr + j * 4096));
                unmap_page(virt_addr + j * 4096);
            }
            flush_tlb();
            return NULL;
        }
    }
    if (free_block->size == 0) {
        if (isKernel) {
            kernel_space_end--; // If the free block is 0, it will always be at the end
        } else {
            user_space_end--;
        }
    }

    return virt_addr;
}

bool free_block(void *addr, uint32_t size, bool isKernel) {
    for (unsigned int i = 0; i < size; i++) {
        if (get_physaddr(addr + i * 4096) == NULL) {
            return false;
        }
    }

    vmm_block_t *space = (isKernel ? kernel_space : user_space);
    vmm_block_t *space_end = (isKernel ? kernel_space_end : user_space_end);

    bool vmm_updated = false;
    if (++space_end == space + SPACE_SIZE) {
        for (size_t i = (size_t)(space_end - space); i > 0; --i) {
            vmm_block_t *ptr = &space[i - 1];
            if (ptr->addr + ptr->size * 4096 == addr) {
                ptr->size += size;
                check_upper(ptr, isKernel);
                vmm_updated = true;
                space_end--;
                break;
            }
            if (addr + size * 4096 == ptr->addr) {
                ptr->addr = addr;
                ptr->size += size;
                check_upper(ptr, isKernel);
                vmm_updated = true;
                space_end--;
                break;
            }
        }
        if (!vmm_updated) {
            return false;
        }
    }
    
    if (!vmm_updated) {
        if (isKernel) {
            kernel_space_end++;
        } else {
            user_space_end++;
        }

        space_end->addr = addr;
        space_end->size = size;

        check_upper(space_end, isKernel);
    }

    for (unsigned int i = 0; i < size; i++) {
        free_page(get_physaddr(addr + i * 4096));
        unmap_page(addr + i * 4096);
        
    }
    coalesce();
    flush_tlb();
    return true;
}

void read_kernel_space() {
    for (vmm_block_t *ptr = kernel_space; ptr <= kernel_space_end; ptr++) {
        printf("Address: %X | Size: %u\n", ptr->addr, ptr->size);
    }
}

void read_user_space() {
    for (vmm_block_t *ptr = user_space; ptr <= user_space_end; ptr++) {
        printf("Address: %X | Size: %u\n", ptr->addr, ptr->size);
    }
}

static uint32_t checksum() {
	uint32_t key = 2983;

    vmm_block_t *ptr;
	for (size_t i = 0; i <= (size_t)(kernel_space_end - kernel_space); i++) {
        ptr = kernel_space + i;
        key *= (uint32_t)(ptr->addr) + 1;
        key *= ptr->size;
    }

	return key;
}

bool test_vmm() {
    uint32_t start_key = checksum();

    volatile uint32_t *a = alloc_block(1, true);
    volatile uint32_t *b = alloc_block(2, true);
    volatile uint32_t *c = alloc_block(8, true);

    if (a == NULL || b == NULL || c == NULL) {
        return false;
    }

    for (unsigned int i = 0; i < 128; i++) {
		a[i] = start_key * (i + 1);
		if (a[i] != start_key * (i + 1)) {
			return false;
		}
	}

    for (unsigned int i = 0; i < 256; i++) {
		b[i] = start_key * (i + 1);
		if (b[i] != start_key * (i + 1)) {
			return false;
		}
	}


    free_block((uint32_t *)a, 1, true);
    free_block((uint32_t *)b, 2, true);
    free_block((uint32_t *)c, 8, true);

    if (start_key != checksum()) {
        return false;
    }
    return true;
}