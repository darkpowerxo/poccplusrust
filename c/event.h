#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>

// Event operation types
typedef enum {
    EV_OP_UPSERT = 1,
    EV_OP_DELETE = 2
} ev_op_t;

// Event payload - compact representation
typedef struct {
    uint8_t  table_id;   // 1=orders, 2=users
    uint16_t index;      // position within table
    uint8_t  op;         // ev_op_t
    uint32_t version;    // snapshot version after write
} __attribute__((packed)) event_t;

// Operation type to string conversion
static inline const char* ev_op_to_string(ev_op_t op) {
    switch (op) {
        case EV_OP_UPSERT: return "UPSERT";
        case EV_OP_DELETE: return "DELETE";
        default: return "UNKNOWN";
    }
}

#endif // EVENT_H
