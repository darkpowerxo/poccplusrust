# POC Demo App: Legacy C Shared Memory + New Rust Module

## Overview

This POC demonstrates **Option A: In-Process C ABI Integration** - a fully functional application where legacy C modules and a new Rust module share the same in-process memory and coordinate via an event bus.

## Architecture

- **Shared Memory**: Global arrays for orders and users with atomic versioning
- **Event Bus**: Lock-free MPMC ring buffer for inter-module coordination  
- **C Modules**: Legacy modules that read/write shared memory and publish events
- **Rust Module**: New module that interoperates via FFI, demonstrating seamless integration

## Build Requirements

- GCC compiler with C99 support
- Rust toolchain (cargo, rustc)
- POSIX threads (pthread)
- Make

### On WSL Ubuntu:
```bash
sudo apt update
sudo apt install build-essential
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

## Building

```bash
make all
```

This will:
1. Build the Rust static library (`cargo build --release`)
2. Compile all C source files
3. Link everything into `bin/poc`

## Running

### Basic Demo
```bash
make run
# or
./bin/poc
```

### Test Suite
```bash
make test
# or
./test_runner.sh
```

## Expected Output

```
🔌 POC: Legacy C + Rust Integration Demo
========================================

[INIT] Initializing shared memory pools...
[INIT] Orders: 128 slots, Users: 64 slots
[INIT] Event bus: 1024 capacity
[INIT] Starting module threads...

MODULE=C1 EVT table=1 idx=0 op=UPSERT ver=1 SNAPSHOT id=9000 qty=42 price=125.3
MODULE=RUST EVT table=1 idx=0 op=UPSERT ver=1 READ qty=42 price=125.30
MODULE=C2 EVT table=2 idx=0 op=UPSERT ver=1 SNAPSHOT id=2000 name="User_0"
MODULE=RUST EVT table=1 idx=5 op=UPSERT ver=1 SNAPSHOT id=9005 qty=73 price=198.7
...
```

## Validation Criteria

✅ **Interop**: C modules write → Rust reads correct data  
✅ **Bidirectional**: Rust writes → C modules react correctly  
✅ **Stability**: Runs 60+ seconds without crashes  
✅ **Performance**: Handles 10-100 Hz event frequency  
✅ **Memory Safety**: No panics cross FFI boundary  

## Clean Up

```bash
make clean
```
