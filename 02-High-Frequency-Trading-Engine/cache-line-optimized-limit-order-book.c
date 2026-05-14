#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// 1. Force 64-byte alignment to perfectly fit a CPU L1 Cache Line
// This prevents "False Sharing" and guarantees fetching an order takes 1 CPU cycle
#define CACHE_LINE 64

// 2. The Intrusive Order Node
// Notice there is no 'ticker' or 'price' here to save space. Those live in the Price Level.
typedef struct OrderNode {
    uint64_t order_id;
    uint32_t quantity;
    struct OrderNode* next;
    struct OrderNode* prev;
} __attribute__((aligned(CACHE_LINE))) OrderNode;

// 3. The Price Level (A specific price point containing a queue of orders)
typedef struct PriceLevel {
    uint32_t price;
    uint32_t total_volume;
    OrderNode* head;
    OrderNode* tail;
    struct PriceLevel* next_level;
    struct PriceLevel* prev_level;
} __attribute__((aligned(CACHE_LINE))) PriceLevel;

// 4. Pre-Allocated Memory Pool (Zero Allocation on the Hot Path)
// We pre-allocate millions of these at startup. During trading, we just claim the next available pointer.
typedef struct OrderPool {
    OrderNode* pool;
    size_t capacity;
    size_t next_free_index;
} OrderPool;

OrderPool* init_order_pool(size_t capacity) {
    OrderPool* p = malloc(sizeof(OrderPool));
    // Allocate exactly aligned memory blocks directly from the OS
    posix_memalign((void**)&p->pool, CACHE_LINE, capacity * sizeof(OrderNode));
    p->capacity = capacity;
    p->next_free_index = 0;
    return p;
}

// $O(1)$ allocation bypassing the OS
inline OrderNode* allocate_order_from_pool(OrderPool* p) {
    if (p->next_free_index >= p->capacity) return NULL; // Out of memory
    return &p->pool[p->next_free_index++];
}

// 5. $O(1)$ Order Insertion at the tail of a Price Level
inline void insert_order_at_level(PriceLevel* level, OrderNode* new_order) {
    new_order->next = NULL;
    new_order->prev = level->tail;

    if (level->tail != NULL) {
        level->tail->next = new_order;
    } else {
        // This is the first order at this price level
        level->head = new_order;
    }
    
    level->tail = new_order;
    level->total_volume += new_order->quantity;
}

// 6. $O(1)$ Order Cancellation (Requires the Strategy Engine to hold a pointer to the Node)
inline void cancel_order(PriceLevel* level, OrderNode* target) {
    if (target->prev) target->prev->next = target->next;
    else level->head = target->next; // Target was the head

    if (target->next) target->next->prev = target->prev;
    else level->tail = target->prev; // Target was the tail

    level->total_volume -= target->quantity;
    
    // In a real system, the node is returned to a Lock-Free FreeList in the Memory Pool
}
