# CHTML Framework - Multi-Threaded Architecture

## Overview
This directory contains the multi-threaded asynchronous implementation of the CHTML framework. This approach uses dedicated threads for handling inter-process communication with external modules, keeping UI updates separate from IPC operations.

## Architecture
- Dedicated thread for IPC communication
- Main thread for UI rendering and user input
- Thread-safe data structures with mutex protection
- Asynchronous communication pattern

## Key Features
1. **Dedicated IPC Thread**: Separate thread handles all communication with modules
2. **Thread-Safe Operations**: Uses mutexes to protect shared data structures
3. **True Parallelism**: UI and IPC operations run simultaneously
4. **Responsive UI**: UI remains completely unaffected by IPC delays

## How it Works
- Main thread runs the OpenGL/GLUT application loop
- IPC thread continuously monitors and reads from module pipes
- When IPC thread receives data, it updates shared data structures under mutex protection
- Main thread periodically checks for updates and refreshes UI accordingly

## Compilation
```bash
bash xh.compile.dir.link.+x.sh
```

## Usage
```bash
# Without module (UI only)
./2.multi-threaded-arch-alt.+x ui_test.chtml

# With module
./2.multi-threaded-arch-alt.+x test_enhanced.chtml module/+x/enhanced_game_ipc.+x
```

## Advantages
- Completely non-blocking UI
- Better performance for high-frequency updates
- Clear separation of concerns
- Can handle slow modules without UI impact

## Disadvantages
- More complex debugging (multi-threading issues)
- Higher memory usage due to thread overhead
- Potential race conditions despite mutexes
- Requires careful synchronization