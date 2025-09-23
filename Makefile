# Windows Makefile - supports both MSVC and MSYS64
# Usage: 
#   make msvc     - Build with MSVC (requires Developer Command Prompt)
#   make msys64   - Build with MSYS64 GCC
#   make rust     - Build Rust static library (requires cargo)

MSVC_CL = cl.exe
MSVC_FLAGS = /std:c11 /O2 /W3 /I"c" /DWIN32_LEAN_AND_MEAN
MSVC_LIBS = ws2_32.lib advapi32.lib

MSYS64_CC = C:/msys64/mingw64/bin/gcc.exe
MSYS64_FLAGS = -Wall -Wextra -std=c99 -O2 -pthread -Ic/
MSYS64_LIBS = -lws2_32 -ladvapi32

RUST_LIB = rust/target/release/poc_rust_module.lib
RUST_LIB_MSYS64 = rust/target/release/libpoc_rust_module.a

C_SOURCES = c/globals.c c/bus.c c/mod1.c c/mod2.c c/main.c

.PHONY: all msvc msys64 rust clean run test help

help:
	@echo "Available targets:"
	@echo "  msvc     - Build with MSVC (run from Developer Command Prompt)"
	@echo "  msys64   - Build with MSYS64 GCC"
	@echo "  rust     - Build Rust static library"
	@echo "  clean    - Clean build artifacts"
	@echo "  run      - Run the application"
	@echo "  test     - Run test suite"

all: help

msvc: rust-msvc bin/poc-msvc.exe

msys64: rust-msys64 bin/poc-msys64.exe

rust-msvc:
	@echo "Building Rust library for MSVC..."
	cd rust && cargo build --release

rust-msys64:
	@echo "Building Rust library for MSYS64..."
	cd rust && cargo build --release

bin/poc-msvc.exe: $(C_SOURCES)
	@if not exist bin mkdir bin
	$(MSVC_CL) $(MSVC_FLAGS) $(C_SOURCES) $(RUST_LIB) $(MSVC_LIBS) /Fe:bin/poc-msvc.exe

bin/poc-msys64.exe: $(C_SOURCES)
	@if not exist bin mkdir bin
	$(MSYS64_CC) $(MSYS64_FLAGS) $(C_SOURCES) $(RUST_LIB_MSYS64) $(MSYS64_LIBS) -o bin/poc-msys64.exe

run: bin/poc-msvc.exe
	bin\poc-msvc.exe

test: bin/poc-msvc.exe
	powershell -ExecutionPolicy Bypass -File test_runner.ps1

clean:
	@if exist bin rmdir /s /q bin
	cd rust && cargo clean
