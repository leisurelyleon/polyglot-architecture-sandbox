#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

// 1. The Metadata Header
// This struct sits invisibly in memory *right before* the pointer we return to the user.
typedef struct block_meta {
    size_t size;
    struct block_meta *next;
    int free;
    int magic; // A magic number for debugging memory corruption
} block_meta;

#define META_SIZE sizeof(struct block_meta)
#define MAGIC_NUM 0x77777777

// Global head of our linked list of memory blocks
void *global_base = NULL;

// Thread-safety lock
pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;

// 2. Finding a free block that fits the requested size
block_meta *find_free_block(block_meta **last, size_t size) {
    block_meta *current = global_base;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
}

// 3. Requesting new memory from the OS Kernel via sbrk
block_meta *request_space(block_meta* last, size_t size) {
    block_meta *block;
    // sbrk(0) gives the current top of the heap
    block = sbrk(0);
    
    // Ask the OS to increment the heap pointer by our required size + metadata
    void *request = sbrk(size + META_SIZE);
    
    // (void*)-1 indicates the OS refused to give us memory
    if (request == (void*) -1) {
        return NULL; 
    }

    // Initialize our invisible header
    if (last) {
        last->next = block;
    }
    block->size = size;
    block->next = NULL;
    block->free = 0;
    block->magic = MAGIC_NUM;
    
    return block;
}

// 4. The actual malloc implementation
void *my_malloc(size_t size) {
    block_meta *block;

    if (size <= 0) return NULL;

    pthread_mutex_lock(&global_malloc_lock);

    if (!global_base) { // First call ever
        block = request_space(NULL, size);
        if (!block) {
            pthread_mutex_unlock(&global_malloc_lock);
            return NULL;
        }
        global_base = block;
    } else {
        block_meta *last = global_base;
        block = find_free_block(&last, size);
        if (!block) { // Failed to find a recycled block, request more from OS
            block = request_space(last, size);
            if (!block) {
                pthread_mutex_unlock(&global_malloc_lock);
                return NULL;
            }
        } else {      // Found a recycled block!
            block->free = 0;
            block->magic = MAGIC_NUM;
        }
    }

    pthread_mutex_unlock(&global_malloc_lock);
    
    // 5. The dangerous pointer math
    // We return a pointer to the memory *after* our metadata header
    return (block + 1); 
}

// 6. The actual free implementation
void my_free(void *ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&global_malloc_lock);

    // Cast the pointer backward in memory to locate our metadata header
    block_meta* block_ptr = (block_meta*)ptr - 1;

    // Check for memory corruption
    if (block_ptr->magic == MAGIC_NUM) {
        block_ptr->free = 1;
    } else {
        printf("CRITICAL ERROR: Memory Corruption Detected during free()!\n");
    }

    pthread_mutex_unlock(&global_malloc_lock);
}
