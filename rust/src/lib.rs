mod ffi;
mod module;

use std::sync::mpsc;
use std::sync::Mutex;
use std::thread::{self, JoinHandle};

// Global state for thread management
static SHUTDOWN_CHANNELS: Mutex<Option<(mpsc::Sender<()>, mpsc::Sender<()>)>> = Mutex::new(None);
static THREAD_HANDLES: Mutex<Option<(JoinHandle<()>, JoinHandle<()>)>> = Mutex::new(None);

/// Initialize the Rust module - called from C
#[no_mangle]
pub extern "C" fn rust_module_init() {
    // Set up panic handling to prevent panics from crossing FFI boundary
    module::handle_panic();
    
    println!("MODULE=RUST INIT starting rust module threads");
    
    // Create shutdown channels
    let (reader_shutdown_tx, reader_shutdown_rx) = mpsc::channel();
    let (writer_shutdown_tx, writer_shutdown_rx) = mpsc::channel();
    
    // Store shutdown senders for later use
    {
        let mut channels = SHUTDOWN_CHANNELS.lock().unwrap();
        *channels = Some((reader_shutdown_tx, writer_shutdown_tx));
    }
    
    // Start reader thread
    let reader_handle = thread::Builder::new()
        .name("rust-reader".to_string())
        .spawn(move || {
            // Wrap in catch_unwind to prevent panics from crossing FFI boundary
            let result = std::panic::catch_unwind(|| {
                module::reader_loop(reader_shutdown_rx);
            });
            
            if let Err(panic) = result {
                eprintln!("MODULE=RUST ERROR: Reader thread panicked: {:?}", panic);
                eprintln!("MODULE=RUST ERROR: Reader thread terminated safely");
            }
        })
        .expect("Failed to create reader thread");
    
    // Start writer thread
    let writer_handle = thread::Builder::new()
        .name("rust-writer".to_string())
        .spawn(move || {
            // Wrap in catch_unwind to prevent panics from crossing FFI boundary
            let result = std::panic::catch_unwind(|| {
                module::writer_loop(writer_shutdown_rx);
            });
            
            if let Err(panic) = result {
                eprintln!("MODULE=RUST ERROR: Writer thread panicked: {:?}", panic);
                eprintln!("MODULE=RUST ERROR: Writer thread terminated safely");
            }
        })
        .expect("Failed to create writer thread");
    
    // Store thread handles for cleanup
    {
        let mut handles = THREAD_HANDLES.lock().unwrap();
        *handles = Some((reader_handle, writer_handle));
    }
    
    println!("MODULE=RUST INIT rust module threads started successfully");
}

/// Shutdown the Rust module gracefully - called from C
#[no_mangle]
pub extern "C" fn rust_module_shutdown() {
    println!("MODULE=RUST SHUTDOWN initiating graceful shutdown");
    
    // Send shutdown signals
    {
        let mut channels = SHUTDOWN_CHANNELS.lock().unwrap();
        if let Some((reader_tx, writer_tx)) = channels.take() {
            let _ = reader_tx.send(());
            let _ = writer_tx.send(());
        }
    }
    
    // Wait for threads to complete
    {
        let mut handles = THREAD_HANDLES.lock().unwrap();
        if let Some((reader_handle, writer_handle)) = handles.take() {
            
            // Wait for reader thread
            if let Err(e) = reader_handle.join() {
                eprintln!("MODULE=RUST ERROR: Failed to join reader thread: {:?}", e);
            } else {
                println!("MODULE=RUST SHUTDOWN reader thread joined successfully");
            }
            
            // Wait for writer thread  
            if let Err(e) = writer_handle.join() {
                eprintln!("MODULE=RUST ERROR: Failed to join writer thread: {:?}", e);
            } else {
                println!("MODULE=RUST SHUTDOWN writer thread joined successfully");
            }
        }
    }
    
    println!("MODULE=RUST SHUTDOWN graceful shutdown completed");
}

/// Emergency shutdown - for testing panic scenarios
#[no_mangle]
pub extern "C" fn rust_module_emergency_shutdown() {
    eprintln!("MODULE=RUST EMERGENCY emergency shutdown initiated");
    
    // Force unlock mutexes if they're poisoned
    let _ = SHUTDOWN_CHANNELS.lock();
    let _ = THREAD_HANDLES.lock();
    
    // Note: In a real emergency, threads might be left running
    // This is acceptable for testing panic recovery
    eprintln!("MODULE=RUST EMERGENCY emergency shutdown completed");
}

/// Get module status - for debugging/monitoring
#[no_mangle]
pub extern "C" fn rust_module_status() -> i32 {
    let channels_exist = SHUTDOWN_CHANNELS.lock()
        .map(|guard| guard.is_some())
        .unwrap_or(false);
    
    let threads_exist = THREAD_HANDLES.lock()
        .map(|guard| guard.is_some())
        .unwrap_or(false);
    
    match (channels_exist, threads_exist) {
        (true, true) => 1,   // Running
        (false, false) => 0, // Stopped
        _ => -1,             // Inconsistent state
    }
}

// Unit tests
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_module_status_initially_stopped() {
        let status = rust_module_status();
        assert_eq!(status, 0); // Should be stopped initially
    }
    
    // Note: Integration tests for init/shutdown require the C runtime
    // and are better handled in the main test suite
}
