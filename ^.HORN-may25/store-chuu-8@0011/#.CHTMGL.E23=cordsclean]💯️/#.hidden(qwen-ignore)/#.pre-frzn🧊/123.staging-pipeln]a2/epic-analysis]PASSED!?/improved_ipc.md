# Improved IPC System for CHTML Framework

## Current Limitations
- Fixed format: `TYPE,x,y,size,r,g,b,a`
- No support for variable-size arrays
- No labeled items
- Only 2D coordinates (no z-axis)
- Limited to shape rendering data
- No support for complex data structures

## Goal
Create an extensible IPC system that can accommodate:
- Dynamic webpages with complex state
- Simple 2D games with a few variables
- Full AAA games with tons of different variables

## Proposed Architecture

### 1. Message-Based Protocol
Instead of fixed CSV lines, use a structured binary protocol with message types:

```c
typedef enum {
    IPC_MSG_TYPE_SHAPE = 1,        // Basic shape rendering data
    IPC_MSG_TYPE_VARIABLE = 2,     // Generic variable with label
    IPC_MSG_TYPE_ARRAY = 3,        // Variable-size array
    IPC_MSG_TYPE_CUSTOM = 4,       // Custom data structure
    IPC_MSG_TYPE_TEXT = 5,         // Text data
    IPC_MSG_TYPE_UI_UPDATE = 6,    // UI update commands
    IPC_MSG_TYPE_COMMAND = 7,      // Commands from UI to module
} ipc_msg_type_t;
```

### 2. Message Structure
```
[Header: 16 bytes][Payload: variable size]
```

Header format:
- Magic number (4 bytes) - for integrity verification
- Message type (4 bytes)
- Data size (4 bytes) - size of the payload
- Message ID (4 bytes) - for synchronization

### 3. Data Types Support
```c
typedef enum {
    IPC_DATA_TYPE_INT = 1,
    IPC_DATA_TYPE_FLOAT = 2,
    IPC_DATA_TYPE_STRING = 3,
    IPC_DATA_TYPE_BOOL = 4,
    IPC_DATA_TYPE_INT_ARRAY = 5,
    IPC_DATA_TYPE_FLOAT_ARRAY = 6,
    IPC_DATA_TYPE_CUSTOM = 7
} ipc_data_type_t;
```

### 4. Specific Message Types

#### Shape Message (for backward compatibility)
```c
typedef struct {
    char type[32];         // e.g., "SQUARE", "CIRCLE", "TRIANGLE"
    float x, y, z;         // 3D coordinates
    float width, height, depth;  // Dimensions
    float color[4];        // RGBA color values
    int32_t id;            // Unique identifier
    char label[64];        // Label for identification
} ipc_shape_data_t;
```

#### Variable Message
```c
typedef struct {
    char label[64];        // Variable label/name
    uint32_t data_type;    // Data type
    uint32_t data_size;    // Size of value in bytes
    union {
        int32_t int_val;
        float float_val;
        uint8_t bool_val;
        char string_val[256];  // For small strings
    } value;
} ipc_variable_t;
```

#### Array Message
```c
typedef struct {
    char label[64];        // Array label/name
    uint32_t data_type;    // Element type
    uint32_t count;        // Number of elements
    uint32_t element_size; // Size of each element in bytes
    // Variable size array data follows after this struct
} ipc_array_header_t;
```

### 5. Implementation Steps

#### Phase 1: Protocol Definition
- Define all message structures and enums
- Create serialization/deserialization functions
- Maintain backward compatibility for basic shapes

#### Phase 2: Model.c Updates
- Replace current CSV parsing with binary message parsing
- Update the update_model() function to handle new message types
- Add support for multiple message types in a single update

#### Phase 3: Module Updates
- Create example modules using the new protocol
- Show how to send different types of messages
- Demonstrate variable-size arrays and labeled items

#### Phase 4: View.c Updates
- Update rendering to work with new shape format (now with z-axis)
- Create flexible rendering for variable data types
- Add UI update mechanisms

### 6. Benefits
- More flexible data transmission
- Support for complex game states
- Backward compatibility for existing shape-based rendering
- Extensible for future needs
- Better performance with binary protocol
- Support for large variable-size arrays

### 7. Migration Path
- Start with shape messages to maintain compatibility
- Gradually add support for variable and array messages
- Existing game modules can continue working with minimal changes
- New modules can take advantage of the full feature set

### 8. Example Usage

Simple shape transmission (current):
```
SQUARE,50,50,20,0.0,0.0,1.0,1.0
```

With new protocol:
```
[Message with TYPE_SHAPE containing 3D position, full RGBA, label, etc]
```

Complex data transmission (new):
```
[Message with TYPE_ARRAY containing a variable-size array of player positions]
[Message with TYPE_VARIABLE containing a labeled score value]
[Message with TYPE_TEXT containing dynamic UI text]
```

This architecture provides a solid foundation for simple applications while scaling to complex AAA games with diverse data needs.