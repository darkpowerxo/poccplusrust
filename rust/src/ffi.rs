use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};

// C struct representations - must match C exactly
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Order {
    pub id: u64,
    pub version: u32,
    pub qty: i32,
    pub price: f32,
    pub padding: [u8; 4],
}

#[repr(C)]  
#[derive(Debug, Clone, Copy)]
pub struct User {
    pub id: u64,
    pub version: u32,
    pub name: [u8; 32],
    pub padding: [u8; 4],
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Event {
    pub table_id: u8,
    pub index: u16,
    pub op: u8,
    pub version: u32,
}

// Constants matching C definitions
pub const TABLE_ID_ORDERS: u8 = 1;
pub const TABLE_ID_USERS: u8 = 2;
pub const EV_OP_UPSERT: u8 = 1;
pub const EV_OP_DELETE: u8 = 2;
pub const ORDERS_CAP: usize = 128;
pub const USERS_CAP: usize = 64;

// External C symbols
extern "C" {
    static mut g_orders: [Order; ORDERS_CAP];
    static mut g_users: [User; USERS_CAP];
    static g_running: AtomicBool;
    static g_events_published: AtomicU32;
    static g_events_consumed: AtomicU32;
    
    fn bus_publish(ev: *const Event) -> i32;
    fn bus_try_consume(out: *mut Event) -> i32;
}

// Safe wrapper functions with proper error handling
pub fn publish_event(ev: Event) -> Result<bool, &'static str> {
    let result = unsafe { bus_publish(&ev as *const Event) };
    match result {
        0 => Ok(false), // Success, no drops
        1 => {
            eprintln!("Warning: Event dropped due to bus overflow");
            Ok(true) // Success but dropped oldest
        },
        _ => Err("Bus publish failed"),
    }
}

pub fn try_consume_event() -> Option<Event> {
    let mut ev = Event { table_id: 0, index: 0, op: 0, version: 0 };
    let result = unsafe { bus_try_consume(&mut ev as *mut Event) };
    if result == 1 { Some(ev) } else { None }
}

pub fn read_order(index: usize) -> Option<Order> {
    if index >= ORDERS_CAP { return None; }
    let order = unsafe { g_orders[index] };
    if order.id == 0 { None } else { Some(order) }
}

pub fn write_order(index: usize, order: Order) -> Result<(), &'static str> {
    if index >= ORDERS_CAP { return Err("Index out of bounds"); }
    
    unsafe {
        g_orders[index] = order;
        // Ensure write is visible before event publication
        std::sync::atomic::fence(Ordering::Release);
    }
    Ok(())
}

pub fn read_user(index: usize) -> Option<User> {
    if index >= USERS_CAP { return None; }
    let user = unsafe { g_users[index] };
    if user.id == 0 { None } else { Some(user) }
}

pub fn write_user(index: usize, user: User) -> Result<(), &'static str> {
    if index >= USERS_CAP { return Err("Index out of bounds"); }
    
    unsafe {
        g_users[index] = user;
        // Ensure write is visible before event publication
        std::sync::atomic::fence(Ordering::Release);
    }
    Ok(())
}

pub fn is_running() -> bool {
    unsafe { g_running.load(Ordering::Relaxed) }
}

// Helper functions for safe string handling
pub fn safe_str_from_user_name(name: &[u8; 32]) -> String {
    let len = name.iter().position(|&b| b == 0).unwrap_or(name.len());
    String::from_utf8_lossy(&name[..len]).to_string()
}

pub fn safe_str_to_user_name(s: &str) -> [u8; 32] {
    let mut name = [0u8; 32];
    let bytes = s.as_bytes();
    let len = std::cmp::min(bytes.len(), 31); // Leave room for null terminator
    name[..len].copy_from_slice(&bytes[..len]);
    name
}

// Error handling - convert Rust errors to C-compatible error codes
pub fn handle_error(err: &str) -> i32 {
    eprintln!("RUST FFI Error: {}", err);
    -1
}

// Memory ordering utilities
pub fn memory_fence_acquire() {
    std::sync::atomic::fence(Ordering::Acquire);
}

pub fn memory_fence_release() {
    std::sync::atomic::fence(Ordering::Release);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_safe_string_conversion() {
        let test_str = "Alice_42";
        let name_array = safe_str_to_user_name(test_str);
        let recovered = safe_str_from_user_name(&name_array);
        assert_eq!(recovered, test_str);
    }

    #[test]
    fn test_long_string_truncation() {
        let long_str = "ThisIsAVeryLongUserNameThatShouldBeTruncated";
        let name_array = safe_str_to_user_name(long_str);
        let recovered = safe_str_from_user_name(&name_array);
        assert!(recovered.len() <= 31); // Should be truncated
    }
}
