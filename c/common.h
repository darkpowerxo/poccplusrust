#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdatomic.h>

#ifdef _WIN32
#include <windows.h>
#define usleep(x) Sleep((x)/1000)
#define sleep(x) Sleep((x)*1000)
#else
#include <unistd.h>
#endif

// Table identifiers
#define TABLE_ID_ORDERS 1
#define TABLE_ID_USERS  2

// Capacity constants
#define ORDERS_CAP 128
#define USERS_CAP   64

// Fixed-size records with monotonic versioning
typedef struct {
    uint64_t id;        // business key (0 = empty slot)
    uint32_t version;   // monotonic per-record
    int32_t  qty;
    float    price;
    char     padding[4]; // Ensure consistent layout
} __attribute__((packed)) order_t;

typedef struct {
    uint64_t id;        // business key (0 = empty slot)  
    uint32_t version;   // monotonic per-record
    char     name[32];
    char     padding[4]; // Alignment
} __attribute__((packed)) user_t;

// Global shared arrays (defined in globals.c)
extern order_t g_orders[ORDERS_CAP];
extern user_t  g_users[USERS_CAP];
extern atomic_bool g_running;

// Statistics tracking (defined in globals.c)
extern atomic_uint g_events_published;
extern atomic_uint g_events_consumed;

#endif // COMMON_H
