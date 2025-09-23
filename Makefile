CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -pthread -fPIC
RUST_LIB = rust/target/release/libpoc_rust_module.a

.PHONY: all clean run test rust

all: bin/poc

rust:
	cd rust && cargo build --release

bin/poc: rust $(wildcard c/*.c c/*.h)
	mkdir -p bin
	$(CC) $(CFLAGS) -Ic/ c/*.c $(RUST_LIB) -ldl -lm -o bin/poc

run: bin/poc
	./bin/poc

test: bin/poc
	./test_runner.sh

clean:
	rm -rf bin/
	cd rust && cargo clean
