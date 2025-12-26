# unilog

Universal embedded logging library for C11 (WIP)

## Overview

`unilog` is a lightweight, lock-free logging library designed for embedded systems. It provides safe logging from any context, including interrupt service routines (ISRs), using an atomic Multiple Producer Single Consumer (MPSC) ring buffer.

## Key Features

- **Lock-Free & Interrupt-Safe**: Safe to call from any context, including ISRs
- **No Dynamic Allocation**: Works entirely on user-provided buffers
- **C11 Standard**: Uses C11 atomics for portability and correctness
- **MPSC Ring Buffer**: Efficient atomic ring buffer for multiple producers, single consumer
- **Formatted Logging**: Supports printf-style formatting via `snprintf`
- **Raw Logging**: Direct message logging with `memcpy`
- **Log Levels**: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- **Minimal Dependencies**: Only requires standard C library functions

## Requirements

- C11 compiler with `<stdatomic.h>` support
- CMake 3.10 or later (for building)

## Building

```bash
mkdir build && cd build
cmake ..
make
```

### Build Options

- `UNILOG_BUILD_EXAMPLES=ON/OFF` - Build example programs (default: ON)
- `UNILOG_BUILD_TESTS=ON/OFF` - Build test programs (default: ON)

## Usage

### Basic Example

```c
#include <unilog/unilog.h>

/* Allocate buffer (no dynamic allocation) */
uint8_t log_buffer[1024];
unilog_t logger;

/* Initialize */
unilog_init(&logger, log_buffer, sizeof(log_buffer));

/* Set minimum log level */
unilog_set_level(&logger, UNILOG_LEVEL_INFO);

/* Write formatted log messages */
unilog_write(&logger, UNILOG_LEVEL_INFO, timestamp, "System started");
unilog_format(&logger, UNILOG_LEVEL_ERROR, timestamp, "Error code: 0x%X", error);

/* Write raw messages */
const char *msg = "Raw message";
unilog_write_raw(&logger, UNILOG_LEVEL_WARN, timestamp, msg, strlen(msg));

/* Read messages (consumer side) */
char buffer[256];
unilog_level_t level;
uint32_t timestamp;

while (unilog_read(&logger, &level, &timestamp, buffer, sizeof(buffer)) > 0) {
    printf("[%u] %s: %s\n", timestamp, unilog_level_name(level), buffer);
}
```

### Interrupt-Safe Example

```c
/* Shared logger - can be accessed from ISRs and main thread */
static unilog_t g_logger;

void timer_isr(void) {
    /* Safe to call from interrupt context */
    unilog_write(&g_logger, UNILOG_LEVEL_DEBUG, get_tick_count(), 
                 "ISR triggered");
}

int main(void) {
    uint8_t buffer[2048];
    unilog_init(&g_logger, buffer, sizeof(buffer));
    
    /* Main loop reads and processes logs */
    while (1) {
        char msg[256];
        unilog_level_t level;
        uint32_t ts;
        
        if (unilog_read(&g_logger, &level, &ts, msg, sizeof(msg)) > 0) {
            /* Process log message */
            transmit_to_host(level, ts, msg);
        }
    }
}
```

## API Reference

### Initialization

- `unilog_init()` - Initialize logger with user-provided buffer
- `unilog_set_level()` - Set minimum log level (atomic)
- `unilog_get_level()` - Get current minimum log level

### Writing

- `unilog_write()` - Write formatted log message (uses `snprintf`)
- `unilog_write_raw()` - Write raw message (uses `memcpy`)

Both functions are:
- Lock-free and interrupt-safe
- Return immediately (non-blocking)
- Return `UNILOG_ERR_FULL` if buffer is full

### Reading

- `unilog_read()` - Read next log entry (consumer only)
- `unilog_available()` - Get bytes available to read
- `unilog_is_empty()` - Check if buffer is empty

### Utilities

- `unilog_level_name()` - Get string name for log level

## Design

### Lock-Free MPSC Buffer

The library uses a lock-free ring buffer with atomic operations:

- **Multiple Producers**: Any thread/ISR can write logs concurrently
- **Single Consumer**: One thread reads and processes logs
- **Atomic Operations**: C11 `stdatomic.h` for thread/interrupt safety
- **No Locks**: No mutexes or spin locks - safe for real-time systems

### Memory Layout

```
Ring Buffer:
┌─────────────────────────────────────┐
│ [Header][Message][Pad][Header][Msg] │
│  └─read_pos          └─write_pos    │
└─────────────────────────────────────┘

Entry Format:
┌────────┬───────┬───────────┬─────────┬─────┐
│ Length │ Level │ Timestamp │ Message │ Pad │
│ 4 bytes│4 bytes│  4 bytes  │ N bytes │ 0-3 │
└────────┴───────┴───────────┴─────────┴─────┘
```

### Buffer Size

- Must be a power of 2 (e.g., 256, 512, 1024, 2048)
- Larger buffers reduce chance of overflow during burst logging
- Recommended: At least 1KB for typical embedded applications

## Testing

Run tests:

```bash
cd build
ctest --verbose
```

Run examples:

```bash
./examples/basic_example
./examples/interrupt_example
```

## Integration

### CMake

```cmake
add_subdirectory(path/to/unilog)
target_link_libraries(your_target PRIVATE unilog)
```

### Manual

Copy `include/unilog/` and `src/unilog.c` to your project and compile with C11 support:

```bash
gcc -std=c11 -c unilog.c -I./include
```

## Performance Characteristics

- **Write Operation**: O(1), lock-free, wait-free in success case
- **Read Operation**: O(1), single consumer
- **Memory**: No dynamic allocation, fixed buffer size
- **Interrupt Latency**: Minimal - just atomic operations and memory copy

## Limitations

- Buffer must be power of 2 size
- Single consumer only (multiple consumers not supported)
- Messages larger than half buffer size are rejected
- No automatic buffer overflow handling - messages are dropped when full

## License

This project is dual-licensed
- AGPL v3 (See LICENSE file)
- Commercial and evaluation licenses on request

## Contributing

This project does NOT accept code contributions, but feel free to open issues
