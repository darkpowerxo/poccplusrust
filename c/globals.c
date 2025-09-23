#include "common.h"
#include "bus.h"
#include <string.h>

// Global shared memory arrays
order_t g_orders[ORDERS_CAP];
user_t  g_users[USERS_CAP];

// Global runtime control
atomic_bool g_running = ATOMIC_VAR_INIT(false);

// Statistics tracking  
atomic_uint g_events_published = ATOMIC_VAR_INIT(0);
atomic_uint g_events_consumed = ATOMIC_VAR_INIT(0);

// Global event bus instance
bus_t g_bus;

// Initialize all global state
void globals_init(void) {
    // Clear shared memory arrays
    memset(g_orders, 0, sizeof(g_orders));
    memset(g_users, 0, sizeof(g_users));
    
    // Initialize event bus
    bus_init();
    
    // Reset statistics
    atomic_store(&g_events_published, 0);
    atomic_store(&g_events_consumed, 0);
    
    // Enable runtime
    atomic_store(&g_running, true);
}

// Cleanup global state
void globals_cleanup(void) {
    atomic_store(&g_running, false);
}
