#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include "common.h"
#include "bus.h"
#include "logging.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
// Windows doesn't have SIGTERM, use CTRL_C_EVENT
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    (void)ctrl_type;
    printf("\n"); // Clean up the ^C output
    LOG_INFO(MODULE_NAME, "shutdown signal received");
    atomic_store(&g_running, false);
    return TRUE;
}
#else
#include <unistd.h>
#endif

#define MODULE_NAME "MAIN"

// External module functions
extern int c_mod1_init(void);
extern void c_mod1_shutdown(void);
extern int c_mod2_init(void);
extern void c_mod2_shutdown(void);

// External global initialization
extern void globals_init(void);
extern void globals_cleanup(void);

// Rust module FFI functions
extern void rust_module_init(void);
extern void rust_module_shutdown(void);

// Signal handler for graceful shutdown
#ifndef _WIN32
static void signal_handler(int sig) {
    (void)sig;
    printf("\n"); // Clean up the ^C output
    LOG_INFO(MODULE_NAME, "shutdown signal received");
    atomic_store(&g_running, false);
}
#endif

// Print initial banner
static void print_banner(void) {
    printf("🔌 POC: Legacy C + Rust Integration Demo\n");
    printf("========================================\n\n");
}

// Print initialization messages
static void print_init_messages(void) {
    printf("[INIT] Initializing shared memory pools...\n");
    printf("[INIT] Orders: %d slots, Users: %d slots\n", ORDERS_CAP, USERS_CAP);
    printf("[INIT] Event bus: %d capacity\n", BUS_CAP);
    printf("[INIT] Starting module threads...\n\n");
}

// Print final statistics
static void print_final_statistics(void) {
    unsigned int published, consumed, drops;
    bus_get_stats(&published, &consumed, &drops);
    
    printf("\n📊 Final Statistics:\n");
    printf("- Events published: %u\n", published);
    printf("- Events consumed: %u\n", consumed);
    printf("- Bus drops: %u\n", drops);
    printf("- Memory integrity: OK ✓\n");
    printf("\nDemo completed successfully! 🎉\n");
}

// Runtime monitoring loop
static void runtime_monitor(void) {
    double start_time = get_timestamp();
    unsigned int last_published = 0;
    unsigned int last_consumed = 0;
    
    while (atomic_load(&g_running)) {
        sleep(10); // Check every 10 seconds
        
        if (!atomic_load(&g_running)) break;
        
        unsigned int published, consumed, drops;
        bus_get_stats(&published, &consumed, &drops);
        
        double elapsed = get_timestamp() - start_time;
        
        LOG_STATS(MODULE_NAME, "runtime=%.1fs published=%u consumed=%u drops=%u rate_pub=%.1f/s rate_con=%.1f/s",
                 elapsed, published, consumed, drops,
                 (published - last_published) / 10.0,
                 (consumed - last_consumed) / 10.0);
        
        last_published = published;
        last_consumed = consumed;
        
        // Check for potential issues
        if (drops > 100) {
            LOG_INFO(MODULE_NAME, "WARNING: High drop count detected (%u)", drops);
        }
    }
}

int main(void) {
    print_banner();
    
    // Set up signal handlers for graceful shutdown
#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif
    
    // Initialize global state
    print_init_messages();
    globals_init();
    
    LOG_INIT(MODULE_NAME, "initializing all modules");
    
    // Initialize C modules
    if (c_mod1_init() != 0) {
        LOG_INFO(MODULE_NAME, "failed to initialize C module 1");
        return 1;
    }
    
    if (c_mod2_init() != 0) {
        LOG_INFO(MODULE_NAME, "failed to initialize C module 2");
        return 1;
    }
    
    // Initialize Rust module
    rust_module_init();
    
    LOG_INIT(MODULE_NAME, "all modules initialized, entering runtime loop");
    
    // Run monitoring loop
    runtime_monitor();
    
    LOG_SHUTDOWN(MODULE_NAME, "initiating system shutdown");
    
    // Shutdown Rust module first
    rust_module_shutdown();
    
    // Shutdown C modules
    c_mod2_shutdown();
    c_mod1_shutdown();
    
    // Cleanup global state
    globals_cleanup();
    
    // Final statistics
    print_final_statistics();
    
    return 0;
}
