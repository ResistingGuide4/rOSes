#ifndef HEAP_H
#define HEAP_H 1

#include <stdint.h>
#include <stdbool.h>

typedef struct heap_header heap_header_t;

typedef enum {
    HEAP_FREE = 0x46524545, //Represents string literal "FREE"
    HEAP_USED = 0x55534544 //Represents string literal "USED"
} heap_status_t;

struct __attribute__((packed)) heap_header {
    uint32_t size; //Size of usable block, not including header
    heap_status_t status; //Represents either one of the string literals "FREE" or "USED"
    heap_header_t *next;
    heap_header_t *prev;
}; //Note: kmalloc requires the size of this to be a multiple of 8 bytes

void *kmalloc(size_t size);
void kfree(void *addr);
void read_heap();

#endif