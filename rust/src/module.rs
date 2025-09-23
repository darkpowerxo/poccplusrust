use crate::ffi::*;
use std::sync::mpsc;
use std::thread;
use std::time::{Duration, Instant};
use std::sync::atomic::{AtomicU32, Ordering};

const MODULE_NAME: &str = "RUST";

// Check if Rust writer is disabled via environment variable
fn is_writer_disabled() -> bool {
    std::env::var("RUST_WRITER_DISABLED").map_or(false, |v| v == "1")
}

// Check if high frequency mode is enabled
fn is_high_frequency() -> bool {
    std::env::var("HIGH_FREQUENCY").map_or(false, |v| v == "1")
}

// Atomic counter for generating unique order IDs
static ORDER_ID_COUNTER: AtomicU32 = AtomicU32::new(8000);

// Reader loop - consumes events and logs read operations
pub fn reader_loop(shutdown_rx: mpsc::Receiver<()>) {
    println!("MODULE={} INIT starting event reader thread", MODULE_NAME);
    
    loop {
        // Check for shutdown signal (non-blocking)
        if shutdown_rx.try_recv().is_ok() {
            break;
        }
        
        // Check global running flag
        if !is_running() {
            break;
        }
        
        // Consume all available events
        let mut events_processed = 0;
        while let Some(ev) = try_consume_event() {
            process_event(ev);
            events_processed += 1;
            
            // Yield occasionally to avoid starving other threads
            if events_processed % 10 == 0 {
                thread::yield_now();
            }
        }
        
        // Brief sleep to avoid busy-waiting
        thread::sleep(Duration::from_millis(2));
    }
    
    println!("MODULE={} SHUTDOWN event reader thread shutting down", MODULE_NAME);
}

// Process a consumed event
fn process_event(ev: Event) {
    match ev.table_id {
        TABLE_ID_ORDERS => {
            if let Some(order) = read_order(ev.index as usize) {
                println!("MODULE={} EVT table={} idx={} op={} ver={} READ id={} qty={} price={:.1}",
                        MODULE_NAME, ev.table_id, ev.index, 
                        op_to_string(ev.op), ev.version,
                        order.id, order.qty, order.price);
            }
        },
        TABLE_ID_USERS => {
            if let Some(user) = read_user(ev.index as usize) {
                let name = safe_str_from_user_name(&user.name);
                println!("MODULE={} EVT table={} idx={} op={} ver={} READ id={} name=\"{}\"",
                        MODULE_NAME, ev.table_id, ev.index,
                        op_to_string(ev.op), ev.version,
                        user.id, name);
            }
        },
        _ => {
            eprintln!("MODULE={} WARNING: Unknown table_id {}", MODULE_NAME, ev.table_id);
        }
    }
}

// Writer loop - periodically writes orders and publishes events  
pub fn writer_loop(shutdown_rx: mpsc::Receiver<()>) {
    if is_writer_disabled() {
        println!("MODULE={} INFO writer disabled via RUST_WRITER_DISABLED=1", MODULE_NAME);
        return;
    }
    
    println!("MODULE={} INIT starting order writer thread", MODULE_NAME);
    
    let mut round_robin_idx = 0;
    let sleep_duration = if is_high_frequency() { 
        Duration::from_millis(15) // ~67Hz
    } else { 
        Duration::from_millis(150) // ~7Hz
    };
    
    let start_time = Instant::now();
    
    loop {
        // Check for shutdown signal (non-blocking)
        if shutdown_rx.try_recv().is_ok() {
            break;
        }
        
        // Check global running flag
        if !is_running() {
            break;
        }
        
        // Generate order at round-robin index
        let idx = round_robin_idx % ORDERS_CAP;
        
        match generate_and_write_order(idx) {
            Ok(order) => {
                // Log successful write
                println!("MODULE={} EVT table={} idx={} op={} ver={} SNAPSHOT id={} qty={} price={:.1}",
                        MODULE_NAME, TABLE_ID_ORDERS, idx,
                        op_to_string(EV_OP_UPSERT), order.version,
                        order.id, order.qty, order.price);
            },
            Err(e) => {
                eprintln!("MODULE={} ERROR: Failed to write order at index {}: {}", MODULE_NAME, idx, e);
            }
        }
        
        round_robin_idx += 1;
        
        // Periodic status update
        if round_robin_idx % 50 == 0 {
            let elapsed = start_time.elapsed().as_secs();
            println!("MODULE={} INFO writer active for {}s, {} orders written", 
                    MODULE_NAME, elapsed, round_robin_idx);
        }
        
        thread::sleep(sleep_duration);
    }
    
    println!("MODULE={} SHUTDOWN order writer thread shutting down", MODULE_NAME);
}

// Generate and write a new order
fn generate_and_write_order(index: usize) -> Result<Order, &'static str> {
    // Generate unique order ID
    let order_id = ORDER_ID_COUNTER.fetch_add(1, Ordering::Relaxed) as u64;
    
    // Generate random-ish values (using simple PRNG to avoid dependencies)
    let qty = 1 + (simple_random() % 100) as i32;
    let price_cents = 10000 + (simple_random() % 50000); // $100.00 to $600.00
    let price = price_cents as f32 / 100.0;
    
    // Read current version and increment
    let current_version = read_order(index).map_or(0, |o| o.version);
    let new_version = current_version + 1;
    
    // Create new order
    let order = Order {
        id: order_id,
        version: new_version,
        qty,
        price,
        padding: [0; 4],
    };
    
    // Write to shared memory
    write_order(index, order)?;
    
    // Publish event
    let ev = Event {
        table_id: TABLE_ID_ORDERS,
        index: index as u16,
        op: EV_OP_UPSERT,
        version: new_version,
    };
    
    match publish_event(ev) {
        Ok(dropped) => {
            if dropped {
                println!("MODULE={} WARNING: Event dropped due to bus overflow", MODULE_NAME);
            }
        },
        Err(e) => {
            eprintln!("MODULE={} ERROR: Failed to publish event: {}", MODULE_NAME, e);
        }
    }
    
    Ok(order)
}

// Simple pseudo-random number generator to avoid dependencies
static mut RNG_STATE: u32 = 0x12345678;

fn simple_random() -> u32 {
    unsafe {
        RNG_STATE = RNG_STATE.wrapping_mul(1103515245).wrapping_add(12345);
        RNG_STATE
    }
}

// Convert operation code to string
fn op_to_string(op: u8) -> &'static str {
    match op {
        EV_OP_UPSERT => "UPSERT",
        EV_OP_DELETE => "DELETE", 
        _ => "UNKNOWN",
    }
}

// Utility function for graceful error handling
pub fn handle_panic() {
    std::panic::set_hook(Box::new(|panic_info| {
        eprintln!("MODULE={} PANIC: {}", MODULE_NAME, panic_info);
        eprintln!("MODULE={} ERROR: Rust panic caught - terminating safely", MODULE_NAME);
    }));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_simple_random() {
        let r1 = simple_random();
        let r2 = simple_random();
        assert_ne!(r1, r2); // Should generate different values
    }

    #[test]
    fn test_op_to_string() {
        assert_eq!(op_to_string(EV_OP_UPSERT), "UPSERT");
        assert_eq!(op_to_string(EV_OP_DELETE), "DELETE");
        assert_eq!(op_to_string(99), "UNKNOWN");
    }
}
