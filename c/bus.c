#include "bus.h"
#include "common.h"
#include <string.h>

// Initialize the event bus
void bus_init(void) {
    memset(&g_bus, 0, sizeof(g_bus));
    atomic_store(&g_bus.head, 0);
    atomic_store(&g_bus.tail, 0);
    atomic_store(&g_bus.drops, 0);
}

// Publish an event to the bus
// Returns 0=success, 1=dropped_oldest
int bus_publish(const event_t *ev) {
    if (!ev) return -1;
    
    unsigned int head = atomic_load_explicit(&g_bus.head, memory_order_relaxed);
    unsigned int next_head = (head + 1) % BUS_CAP;
    unsigned int tail = atomic_load_explicit(&g_bus.tail, memory_order_acquire);
    
    int dropped = 0;
    
    // Check if buffer is full (next_head would equal tail)
    if (next_head == tail) {
        // Drop oldest: advance tail to make room
        unsigned int new_tail = (tail + 1) % BUS_CAP;
        
        // Try to advance tail atomically
        if (atomic_compare_exchange_weak_explicit(&g_bus.tail, &tail, new_tail,
                                                memory_order_release, memory_order_relaxed)) {
            atomic_fetch_add_explicit(&g_bus.drops, 1, memory_order_relaxed);
            dropped = 1;
        } else {
            // Another thread advanced tail, retry with new values
            tail = atomic_load_explicit(&g_bus.tail, memory_order_acquire);
            if (next_head == tail) {
                // Still full, bail out
                return 1;
            }
        }
    }
    
    // Write event to buffer
    g_bus.buf[head] = *ev;
    
    // Advance head with release semantics to make write visible
    atomic_store_explicit(&g_bus.head, next_head, memory_order_release);
    
    // Update statistics
    atomic_fetch_add_explicit(&g_events_published, 1, memory_order_relaxed);
    
    return dropped;
}

// Try to consume an event from the bus
// Returns 1=got_event, 0=empty
int bus_try_consume(event_t *out) {
    if (!out) return -1;
    
    unsigned int tail = atomic_load_explicit(&g_bus.tail, memory_order_relaxed);
    unsigned int head = atomic_load_explicit(&g_bus.head, memory_order_acquire);
    
    // Check if buffer is empty
    if (tail == head) {
        return 0;
    }
    
    // Read event from buffer
    *out = g_bus.buf[tail];
    
    // Advance tail with release semantics
    unsigned int next_tail = (tail + 1) % BUS_CAP;
    atomic_store_explicit(&g_bus.tail, next_tail, memory_order_release);
    
    // Update statistics  
    atomic_fetch_add_explicit(&g_events_consumed, 1, memory_order_relaxed);
    
    return 1;
}

// Get bus statistics
void bus_get_stats(unsigned int *published, unsigned int *consumed, unsigned int *drops) {
    if (published) *published = atomic_load_explicit(&g_events_published, memory_order_relaxed);
    if (consumed) *consumed = atomic_load_explicit(&g_events_consumed, memory_order_relaxed);
    if (drops) *drops = atomic_load_explicit(&g_bus.drops, memory_order_relaxed);
}
