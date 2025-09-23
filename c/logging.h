#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "event.h"

// Consistent logging format: MODULE=<name> EVT table=<id> idx=<n> op=<op> ver=<v> SNAPSHOT <key>=<val> ...

#define LOG_EVENT_READ(module, table_id, index, op, version, ...) \
    printf("MODULE=%s EVT table=%d idx=%d op=%s ver=%u READ " __VA_ARGS__ "\n", \
           module, table_id, index, ev_op_to_string(op), version)

#define LOG_EVENT_SNAPSHOT(module, table_id, index, op, version, ...) \
    printf("MODULE=%s EVT table=%d idx=%d op=%s ver=%u SNAPSHOT " __VA_ARGS__ "\n", \
           module, table_id, index, ev_op_to_string(op), version)

#define LOG_INFO(module, ...) \
    printf("MODULE=%s INFO " __VA_ARGS__ "\n", module)

#define LOG_INIT(module, ...) \
    printf("MODULE=%s INIT " __VA_ARGS__ "\n", module)

#define LOG_SHUTDOWN(module, ...) \
    printf("MODULE=%s SHUTDOWN " __VA_ARGS__ "\n", module)

#define LOG_STATS(module, ...) \
    printf("MODULE=%s STATS " __VA_ARGS__ "\n", module)

// Helper function to get current timestamp
static inline double get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

// Helper to format user name safely
static inline void safe_print_user_name(char *out, size_t out_size, const char *name, size_t name_size) {
    size_t len = 0;
    while (len < name_size && name[len] != '\0' && len < out_size - 1) {
        out[len] = name[len];
        len++;
    }
    out[len] = '\0';
}

#endif // LOGGING_H
