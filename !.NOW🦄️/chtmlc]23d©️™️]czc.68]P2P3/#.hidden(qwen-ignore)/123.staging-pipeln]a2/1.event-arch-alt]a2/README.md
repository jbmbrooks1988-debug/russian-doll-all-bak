# CHTML Framework - Event-Based Architecture

## Overview
This directory contains the event-based asynchronous implementation of the CHTML framework. Instead of using threads, this approach uses event-driven I/O with the `select()` system call to handle non-blocking communication with external modules.

## Architecture
- Uses `select()` system call for non-blocking I/O
- Single-threaded event loop architecture
- No shared memory concerns between threads
- Lower memory overhead compared to multi-threaded approach

## Key Features
1. **Non-blocking IPC**: Uses `select()` to check for available data without blocking the UI thread
2. **Event-driven Design**: The main loop processes events when data is available
3. **Thread-safe**: Being single-threaded, avoids race conditions and synchronization issues
4. **Responsive UI**: UI remains responsive even when communicating with slow modules

## How it Works
- Main thread handles UI rendering and user input
- Event-based checking of pipe availability using `select()`
- When data is available, process it immediately
- When no data is available, continue with UI updates

## Compilation
```bash
bash xh.compile.dir.link.+x.sh
```

## Usage
```bash
# Without module (UI only)
./1.event-arch-alt.+x ui_test.chtml

# With module
./1.event-arch-alt.+x test_enhanced.chtml module/+x/enhanced_game_ipc.+x
```

## Advantages
- Simpler debugging (no multi-threading issues)
- Lower memory usage
- No race conditions
- Deterministic behavior

## Disadvantages
- May not be as performant as multi-threaded approach for high-frequency updates
- Event loop can become complex with many I/O sources