#ifndef BUS_H
#define BUS_H

#include <stdatomic.h>
#include "event.h"

// Ring buffer capacity (power of 2 for efficient modulo)
#define BUS_CAP 1024

// Lock-free MPMC ring buffer
typedef struct {
    event_t      buf[BUS_CAP];
    atomic_uint  head;   // next write position
    atomic_uint  tail;   // next read position  
    atomic_uint  drops;  // overrun counter
} bus_t;

// Global event bus instance (defined in globals.c)
extern bus_t g_bus;

// API functions
// Returns 0=success, 1=dropped_oldest
int bus_publish(const event_t *ev);

// Returns 1=got_event, 0=empty
int bus_try_consume(event_t *out);

// Initialize the bus
void bus_init(void);

// Get statistics
void bus_get_stats(unsigned int *published, unsigned int *consumed, unsigned int *drops);

#endif // BUS_H
