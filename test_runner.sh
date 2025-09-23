#!/bin/bash

echo "🧪 POC Test Suite"
echo "=================="

# Test 1: Smoke test
echo "Test 1: Smoke Test (10s runtime)"
timeout 10s ./bin/poc || echo "FAIL: Smoke test crashed"

# Test 2: C-only writers (disable Rust writer via env var)
echo "Test 2: C Writers Only" 
RUST_WRITER_DISABLED=1 timeout 15s ./bin/poc

# Test 3: Rust-only writer (disable C writers)
echo "Test 3: Rust Writer Only"
C_WRITERS_DISABLED=1 timeout 15s ./bin/poc

# Test 4: High frequency stress test
echo "Test 4: Stress Test (100Hz writes for 30s)"
HIGH_FREQUENCY=1 timeout 30s ./bin/poc

# Test 5: Long-running stability
echo "Test 5: Stability Test (60s)"
timeout 60s ./bin/poc

echo "✅ Test suite completed"
