#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "common.h"
#include "bus.h"
#include "logging.h"

#define MODULE_NAME "C1"

static pthread_t writer_thread;
static pthread_t reader_thread;

// Check if this module's writer is disabled via environment variable
static int is_writer_disabled(void) {
    const char* env = getenv("C_WRITERS_DISABLED");
    return env && strcmp(env, "1") == 0;
}

// Check if high frequency mode is enabled
static int is_high_frequency(void) {
    const char* env = getenv("HIGH_FREQUENCY");
    return env && strcmp(env, "1") == 0;
}

// Writer thread - periodically updates orders and publishes events
void* c_mod1_writer_thread(void* arg) {
    (void)arg;
    
    if (is_writer_disabled()) {
        LOG_INFO(MODULE_NAME, "writer disabled via C_WRITERS_DISABLED=1");
        return NULL;
    }
    
    LOG_INIT(MODULE_NAME, "starting order writer thread");
    
    static int round_robin_idx = 0;
    int sleep_us = is_high_frequency() ? 10000 : 100000; // 100Hz vs 10Hz
    
    // Seed random number generator
    srand((unsigned int)time(NULL) ^ (uintptr_t)pthread_self());
    
    while (atomic_load(&g_running)) {
        // Update order at round-robin index
        int idx = round_robin_idx % ORDERS_CAP;
        
        // Generate new order data
        uint64_t order_id = 9000 + idx;
        int32_t qty = 1 + (rand() % 100);
        float price = 100.0f + (rand() % 500) / 10.0f;
        
        // Write with proper memory ordering
        g_orders[idx].id = order_id;
        g_orders[idx].qty = qty;
        g_orders[idx].price = price;
        
        // Increment version atomically
        uint32_t new_version = atomic_fetch_add(&g_orders[idx].version, 1) + 1;
        
        // Ensure writes are visible before event publication
        atomic_thread_fence(memory_order_release);
        
        // Publish event
        event_t ev = {
            .table_id = TABLE_ID_ORDERS,
            .index = (uint16_t)idx,
            .op = EV_OP_UPSERT,
            .version = new_version
        };
        
        int result = bus_publish(&ev);
        
        // Log the write operation
        LOG_EVENT_SNAPSHOT(MODULE_NAME, TABLE_ID_ORDERS, idx, EV_OP_UPSERT, new_version,
                          "id=%lu qty=%d price=%.1f", order_id, qty, price);
        
        if (result == 1) {
            LOG_INFO(MODULE_NAME, "event dropped due to bus overflow");
        }
        
        round_robin_idx++;
        usleep(sleep_us);
    }
    
    LOG_SHUTDOWN(MODULE_NAME, "order writer thread shutting down");
    return NULL;
}

// Reader thread - consumes events and logs read operations
void* c_mod1_reader_thread(void* arg) {
    (void)arg;
    
    LOG_INIT(MODULE_NAME, "starting event reader thread");
    
    while (atomic_load(&g_running)) {
        event_t ev;
        
        while (bus_try_consume(&ev)) {
            // Only log reads for events from other modules (different operations or external changes)
            if (ev.table_id == TABLE_ID_ORDERS) {
                // Read the current state
                order_t order = g_orders[ev.index];
                
                if (order.id != 0) {
                    LOG_EVENT_READ(MODULE_NAME, ev.table_id, ev.index, ev.op, ev.version,
                                  "id=%lu qty=%d price=%.1f", order.id, order.qty, order.price);
                }
            } else if (ev.table_id == TABLE_ID_USERS) {
                // Read user data
                user_t user = g_users[ev.index];
                
                if (user.id != 0) {
                    char name_buf[33];
                    safe_print_user_name(name_buf, sizeof(name_buf), user.name, sizeof(user.name));
                    LOG_EVENT_READ(MODULE_NAME, ev.table_id, ev.index, ev.op, ev.version,
                                  "id=%lu name=\"%s\"", user.id, name_buf);
                }
            }
        }
        
        // Brief sleep to avoid busy-waiting
        usleep(1000); // 1ms
    }
    
    LOG_SHUTDOWN(MODULE_NAME, "event reader thread shutting down");
    return NULL;
}

// Initialize and start C Module 1 threads
int c_mod1_init(void) {
    LOG_INIT(MODULE_NAME, "initializing module threads");
    
    // Start writer thread
    if (pthread_create(&writer_thread, NULL, c_mod1_writer_thread, NULL) != 0) {
        LOG_INFO(MODULE_NAME, "failed to create writer thread");
        return -1;
    }
    
    // Start reader thread
    if (pthread_create(&reader_thread, NULL, c_mod1_reader_thread, NULL) != 0) {
        LOG_INFO(MODULE_NAME, "failed to create reader thread");
        return -1;
    }
    
    return 0;
}

// Shutdown C Module 1 threads
void c_mod1_shutdown(void) {
    LOG_SHUTDOWN(MODULE_NAME, "initiating graceful shutdown");
    
    // Threads will exit when g_running becomes false
    pthread_join(writer_thread, NULL);
    pthread_join(reader_thread, NULL);
    
    LOG_SHUTDOWN(MODULE_NAME, "all threads stopped");
}
