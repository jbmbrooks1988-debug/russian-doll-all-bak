# CHTML Framework Architecture Patterns - Detailed Code Comparison

This document explains the specific code differences between the three architectures: Original (Synchronous), Event-Based, and Multi-Threaded.

## 1. ORIGINAL (Synchronous) Architecture

### Key Characteristics:
- Blocking I/O operations
- Single-threaded execution
- Direct function calls
- Simple execution flow

### Code Locations of Key Differences:

**Main Loop (`1.main_prototype_a1.c`):**
```c
void idle_func() {
    int updated = update_model();  // This call blocks if no data available
    
    if (updated) {
        update_ui_with_model_variables();
        glutPostRedisplay();
    }
    
    // Fixed sleep time
    usleep(16000); // Sleep for approximately 16 milliseconds
}
```

**Model Update (`2.model.c`):**
```c
int update_model() {
    if (child_pid == -1) {
        return 0; // No module to update from
    }
    
    // This read() call blocks until data is available
    char buffer[1024];
    int bytes_read = read(child_to_parent_pipe[0], buffer, sizeof(buffer) - 1);

    // Process data if read was successful
    if (bytes_read > 0) {
        // ... parsing and processing logic
        return 1; // Model was updated
    }
    return 0; // No update
}
```

### Architecture Pattern:
- **Synchronous Blocking I/O**: The main thread waits for data before proceeding
- **Tight Coupling**: UI updates are directly tied to IPC operations
- **Simple but Blocking**: Can block the UI if the module doesn't respond

---

## 2. EVENT-BASED Architecture

### Key Characteristics:
- Non-blocking I/O using `select()`
- Single-threaded event loop
- Polling for available data
- No blocking operations

### Code Locations of Key Differences:

**Main Loop (`1.main_prototype_a1.c`):**
```c
void idle_func() {
    // Use event-based approach with select() to check for available data
    int updated = update_model_nonblocking();  // Non-blocking check
    
    if (updated) {
        update_ui_with_model_variables();
        glutPostRedisplay();
    }
    
    usleep(1000); // Smaller sleep time
}
```

**Model Update (`2.model.c`):**
```c
int update_model_nonblocking() {
    if (child_pid == -1) {
        return 0; // No module to update from
    }
    
    // Use select() to check if data is available without blocking
    fd_set readfds;
    struct timeval timeout;
    
    FD_ZERO(&readfds);
    FD_SET(child_to_parent_pipe[0], &readfds);
    
    // Set timeout to 0 to make it non-blocking
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    int activity = select(child_to_parent_pipe[0] + 1, &readfds, NULL, NULL, &timeout);
    
    if (activity > 0 && FD_ISSET(child_to_parent_pipe[0], &readfds)) {
        // Data is available, try to read it
        char buffer[1024];
        int bytes_read = read(child_to_parent_pipe[0], buffer, sizeof(buffer) - 1);
        
        // Process data...
        return 1; // Model was updated
    }
    return 0; // No update
}
```

### Architecture Pattern:
- **Event-Driven I/O**: Uses `select()` to check for available data without blocking
- **Polling Model**: Continuously checks for data availability
- **Non-blocking**: UI remains responsive regardless of module state
- **Single Thread**: All operations run in the main thread

---

## 3. MULTI-THREADED Architecture

### Key Characteristics:
- Dedicated thread for IPC communication
- Main thread for UI
- Thread-safe data structures
- Shared state with synchronization

### Code Locations of Key Differences:

**Main Loop (`1.main_prototype_a1.c`):**
```c
void idle_func() {
    // Check if model was updated by the IPC thread
    if (model_updated) {
        model_updated = 0;  // Reset the flag
        update_ui_with_model_variables();
        glutPostRedisplay();
    }
    
    usleep(1000); // Small sleep
}
```

**Model Initialization (`2.model.c`):**
```c
void init_model(const char* module_path) {
    // ... pipe setup code ...
    
    // Start the IPC thread to handle communication with the module
    ipc_thread_running = 1;
    if (pthread_create(&ipc_thread, NULL, ipc_thread_func, NULL) != 0) {
        perror("pthread_create failed");
        ipc_thread_running = 0;
    }
}
```

**IPC Thread Function (`2.model.c`):**
```c
void* ipc_thread_func(void* arg) {
    char buffer[1024];
    int bytes_read;
    
    while (ipc_thread_running) {
        // Try to read from the pipe (non-blocking due to O_NONBLOCK flag)
        bytes_read = read(child_to_parent_pipe[0], buffer, sizeof(buffer) - 1);
        
        if (bytes_read > 0) {
            // Lock mutex before modifying shared data
            pthread_mutex_lock(&model_mutex);
            
            // Process data...
            
            // Unlock mutex
            pthread_mutex_unlock(&model_mutex);
            
            // Signal main thread
            model_updated = 1;
        }
        
        usleep(1000); // Sleep to prevent busy waiting
    }
    
    return NULL;
}

// Simplified update function for main thread
int update_model() {
    int was_updated = model_updated;
    if (was_updated) {
        model_updated = 0;
    }
    return was_updated;
}
```

**Thread-Safe Data (`2.model.c`):**
```c
// Shared state variables
int model_updated = 0;  // Flag set by IPC thread, read by main thread
int ipc_thread_running = 0;

// Mutex for protecting shared data
pthread_mutex_t model_mutex = PTHREAD_MUTEX_INITIALIZER;
```

### Architecture Pattern:
- **Separation of Concerns**: IPC and UI run in separate threads
- **Producer-Consumer**: IPC thread produces data, main thread consumes it
- **Synchronization**: Uses mutexes to protect shared state
- **True Parallelism**: UI and IPC operations can happen simultaneously

---

## SUMMARY OF ARCHITECTURAL PATTERNS

| Pattern | Concurrency Model | Synchronization | Complexity | Performance |
|---------|------------------|-----------------|------------|-------------|
| Original | Single-threaded | None | Low | Can block UI |
| Event-Based | Single-threaded | None | Medium | Non-blocking |
| Multi-Threaded | Multi-threaded | Mutexes | High | Best performance |

### When to Use Each Pattern:
- **Original**: Simple applications, debugging, learning
- **Event-Based**: Real-time UI responsiveness required, moderate complexity
- **Multi-Threaded**: High-performance applications, complex IPC, maximum responsiveness