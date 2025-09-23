#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "common.h"
#include "bus.h"
#include "logging.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
typedef HANDLE thread_t;
typedef unsigned int (__stdcall *thread_func_t)(void *);
#define thread_create(thread, attr, start_routine, arg) \
    ((*thread = (HANDLE)_beginthreadex(NULL, 0, (thread_func_t)start_routine, arg, 0, NULL)) != NULL ? 0 : -1)
#define thread_join(thread, retval) (WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0 ? 0 : -1)
#else
#include <pthread.h>
#include <unistd.h>
typedef pthread_t thread_t;
#define thread_create pthread_create
#define thread_join pthread_join
#endif

#define MODULE_NAME "C2"

static thread_t writer_thread;
static thread_t reader_thread;

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

// Generate user name
static void generate_user_name(char *name_buf, size_t buf_size, int user_idx) {
    const char* name_templates[] = {
        "Alice", "Bob", "Charlie", "Diana", "Eve", "Frank", "Grace", "Henry",
        "Ivy", "Jack", "Kate", "Liam", "Mia", "Noah", "Olivia", "Paul",
        "Quinn", "Rachel", "Sam", "Tina", "Uma", "Victor", "Wendy", "Xavier"
    };
    
    int template_idx = user_idx % (sizeof(name_templates) / sizeof(name_templates[0]));
    snprintf(name_buf, buf_size, "%s_%d", name_templates[template_idx], user_idx);
}

// Writer thread - periodically updates users and publishes events
#ifdef _WIN32
unsigned int __stdcall c_mod2_writer_thread(void* arg) {
#else
void* c_mod2_writer_thread(void* arg) {
#endif
    (void)arg;
    
    if (is_writer_disabled()) {
        LOG_INFO(MODULE_NAME, "writer disabled via C_WRITERS_DISABLED=1");
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    
    LOG_INIT(MODULE_NAME, "starting user writer thread");
    
    static int round_robin_idx = 0;
    int sleep_us = is_high_frequency() ? 25000 : 200000; // 40Hz vs 5Hz (different timing than mod1)
    
    // Seed random number generator differently than mod1
#ifdef _WIN32
    srand((unsigned int)time(NULL) ^ GetCurrentThreadId() ^ 0x12345678);
#else
    srand((unsigned int)time(NULL) ^ (uintptr_t)pthread_self() ^ 0x12345678);
#endif
    
    while (atomic_load(&g_running)) {
        // Update user at round-robin index
        int idx = round_robin_idx % USERS_CAP;
        
        // Generate new user data
        uint64_t user_id = 2000 + idx;
        char name_buf[32];
        generate_user_name(name_buf, sizeof(name_buf), idx);
        
        // Write with proper memory ordering
        g_users[idx].id = user_id;
        memset(g_users[idx].name, 0, sizeof(g_users[idx].name));
        strncpy(g_users[idx].name, name_buf, sizeof(g_users[idx].name) - 1);
        
        // Increment version atomically
        uint32_t new_version = atomic_fetch_add(&g_users[idx].version, 1) + 1;
        
        // Ensure writes are visible before event publication
        atomic_thread_fence(memory_order_release);
        
        // Publish event
        event_t ev = {
            .table_id = TABLE_ID_USERS,
            .index = (uint16_t)idx,
            .op = EV_OP_UPSERT,
            .version = new_version
        };
        
        int result = bus_publish(&ev);
        
        // Log the write operation
        LOG_EVENT_SNAPSHOT(MODULE_NAME, TABLE_ID_USERS, idx, EV_OP_UPSERT, new_version,
                          "id=%llu name=\"%s\"", user_id, name_buf);
        
        if (result == 1) {
            LOG_INFO(MODULE_NAME, "event dropped due to bus overflow");
        }
        
        round_robin_idx++;
        usleep(sleep_us);
    }
    
    LOG_SHUTDOWN(MODULE_NAME, "user writer thread shutting down");
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

// Reader thread - consumes events and logs read operations
#ifdef _WIN32
unsigned int __stdcall c_mod2_reader_thread(void* arg) {
#else
void* c_mod2_reader_thread(void* arg) {
#endif
    (void)arg;
    
    LOG_INIT(MODULE_NAME, "starting event reader thread");
    
    while (atomic_load(&g_running)) {
        event_t ev;
        
        while (bus_try_consume(&ev)) {
            // Log reads for all events to show comprehensive event consumption
            if (ev.table_id == TABLE_ID_ORDERS) {
                // Read order data
                order_t order = g_orders[ev.index];
                
                if (order.id != 0) {
                    LOG_EVENT_READ(MODULE_NAME, ev.table_id, ev.index, ev.op, ev.version,
                                  "id=%llu qty=%d price=%.1f", order.id, order.qty, order.price);
                }
            } else if (ev.table_id == TABLE_ID_USERS) {
                // Read user data
                user_t user = g_users[ev.index];
                
                if (user.id != 0) {
                    char name_buf[33];
                    safe_print_user_name(name_buf, sizeof(name_buf), user.name, sizeof(user.name));
                    LOG_EVENT_READ(MODULE_NAME, ev.table_id, ev.index, ev.op, ev.version,
                                  "id=%llu name=\"%s\"", user.id, name_buf);
                }
            }
        }
        
        // Brief sleep to avoid busy-waiting
        usleep(1500); // 1.5ms (slightly different from mod1)
    }
    
    LOG_SHUTDOWN(MODULE_NAME, "event reader thread shutting down");
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

// Initialize and start C Module 2 threads
int c_mod2_init(void) {
    LOG_INIT(MODULE_NAME, "initializing module threads");
    
    // Start writer thread
    if (thread_create(&writer_thread, NULL, c_mod2_writer_thread, NULL) != 0) {
        LOG_INFO(MODULE_NAME, "failed to create writer thread");
        return -1;
    }
    
    // Start reader thread
    if (thread_create(&reader_thread, NULL, c_mod2_reader_thread, NULL) != 0) {
        LOG_INFO(MODULE_NAME, "failed to create reader thread");
        return -1;
    }
    
    return 0;
}

// Shutdown C Module 2 threads
void c_mod2_shutdown(void) {
    LOG_SHUTDOWN(MODULE_NAME, "initiating graceful shutdown");
    
    // Threads will exit when g_running becomes false
    thread_join(writer_thread, NULL);
    thread_join(reader_thread, NULL);
    
    LOG_SHUTDOWN(MODULE_NAME, "all threads stopped");
}
