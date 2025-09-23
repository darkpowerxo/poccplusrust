#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include "event.h"

// Simplified logging for MSVC compatibility
static inline void log_event_read(const char* module, int table_id, int index, int op, unsigned int version, const char* format, ...) {
    printf("MODULE=%s EVT table=%d idx=%d op=%s ver=%u READ ", module, table_id, index, ev_op_to_string(op), version);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

static inline void log_event_snapshot(const char* module, int table_id, int index, int op, unsigned int version, const char* format, ...) {
    printf("MODULE=%s EVT table=%d idx=%d op=%s ver=%u SNAPSHOT ", module, table_id, index, ev_op_to_string(op), version);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

static inline void log_info(const char* module, const char* format, ...) {
    printf("MODULE=%s INFO ", module);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

static inline void log_init(const char* module, const char* format, ...) {
    printf("MODULE=%s INIT ", module);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

static inline void log_shutdown(const char* module, const char* format, ...) {
    printf("MODULE=%s SHUTDOWN ", module);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

static inline void log_stats(const char* module, const char* format, ...) {
    printf("MODULE=%s STATS ", module);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

#define LOG_EVENT_READ log_event_read
#define LOG_EVENT_SNAPSHOT log_event_snapshot
#define LOG_INFO log_info
#define LOG_INIT log_init
#define LOG_SHUTDOWN log_shutdown
#define LOG_STATS log_stats

// Helper function to get current timestamp
static inline double get_timestamp(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
#endif
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
