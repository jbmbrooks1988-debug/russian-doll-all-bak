# C-HTML Framework Walkthrough

## Overview

The C-HTML (C Hyper-Text Markup Language) framework is a unique system that allows developers to create rich graphical user interfaces using HTML-like markup syntax. Unlike traditional web technologies, this system renders directly to OpenGL graphics and enables complex 3D rendering, real-time interactions, and bidirectional communication between UI elements and custom modules.

## Core Architecture

### 1. Main Components

The system consists of three primary components:

- **Model (2.model_v0.01.c)**: Handles data, communications, and state management
- **View (3.view_v0.01.c)**: Handles rendering and UI element display
- **Controller (4.controller_v0.01.c)**: Handles user input and event processing
- **Main (1.main_prototype_v0.01.c)**: Entry point and application coordination

### 2. Communication Architecture

The system supports two types of module communication:

1. **Pipe-based communication**: Traditional stdin/stdout communication between modules and the main application
2. **Shared memory communication**: Higher-performance communication using shared memory segments and semaphores

### 3. File Structure
```
!.chtmlc.0x41/                    # Main application
├── module/                       # Source modules
│   ├── +x/                      # Compiled modules
│   └── *.c                      # Module source files
├── chtml/                       # CHTML files
├── bak/                         # Backups
├── example/                     # Example files
└── *.c                          # Core source files
```

## Detailed Component Breakdown

### Model (2.model_v0.01.c)

The Model component handles all data management, IPC communication, and module coordination.

#### Key Features:
- **Shape Management**: Manages 3D/2D shapes with properties including position, size, color, and now rotation and scaling
- **Module Management**: Supports multiple modules with both pipe and shared memory protocols
- **Data Parsing**: Handles multiple data formats including old CSV, new SHAPE format, and variable/array formats
- **State Persistence**: Maintains active shapes and variables across module updates

#### Shape Structure:
```c
typedef struct {
    int id;
    char type[32];
    float x, y, z;                    // 3D coordinates
    float width, height, depth;       // Dimensions
    float color[3];
    float alpha;                      // Transparency
    char label[64];                   // Identifier
    int active;                       // Visibility flag
    float rotation_x, rotation_y, rotation_z;  // Rotation angles (NEW)
    float scale_x, scale_y, scale_z;           // Scale factors (NEW)
} Shape;
```

The addition of rotation and scale fields maintains backward compatibility:
- **Backward Compatibility**: When parsing old 12-field format, rotation values default to 0.0 and scale values default to 1.0
- **Forward Compatibility**: New 18-field format includes rotation and scale data
- **Rendering Impact**: Only applied when values differ from defaults to maintain exact behavior for legacy modules

#### Parsing Functions:
- `parse_csv_message()`: Legacy CSV format compatibility
- `parse_enhanced_format()`: New SHAPE format with 12 or 18 fields
- `parse_variable_format()`: Variable definition format
- `parse_array_format()`: Array definition format

#### Module Communication:
- Supports multiple active modules simultaneously
- Each module can use different communication protocols (pipe or shared memory)
- Handles both command sending and response receiving

### View (3.view_v0.01.c)

The View component handles all rendering, UI elements, and visual representation.

#### Key Features:
- **OpenGL-based Rendering**: Direct OpenGL rendering with 2D and 3D capabilities
- **Flexible UI Elements**: Supports windows, buttons, text, canvases, menus, and more
- **3D Transformation Support**: Full 3D rendering with rotation and scaling
- **Emoji Support**: Integrated emoji rendering with FreeType
- **Camera Controls**: Support for 3D perspective and camera positioning

#### Enhanced Rendering with Rotation/Scale:

**3D Shape Rendering:**
```c
// Conditional rendering to maintain backward compatibility
if (s->rotation_x != 0.0f || s->rotation_y != 0.0f || s->rotation_z != 0.0f || 
    s->scale_x != 1.0f || s->scale_y != 1.0f || s->scale_z != 1.0f) {
    glPushMatrix();
    glTranslatef(pos_3d.x, pos_3d.y, pos_3d.z);
    if (s->rotation_x != 0.0f) glRotatef(s->rotation_x, 1.0f, 0.0f, 0.0f);
    if (s->rotation_y != 0.0f) glRotatef(s->rotation_y, 0.0f, 1.0f, 0.0f);
    if (s->rotation_z != 0.0f) glRotatef(s->rotation_z, 0.0f, 0.0f, 1.0f);
    if (s->scale_x != 1.0f || s->scale_y != 1.0f || s->scale_z != 1.0f) 
        glScalef(s->scale_x, s->scale_y, s->scale_z);
    render_3d_rect_prism(0.0f, 0.0f, 0.0f, size_x, size_y, size_z);
    glPopMatrix();
} else {
    // For backward compatibility - render without transformations
    render_3d_rect_prism(pos_3d.x, pos_3d.y, pos_3d.z, size_x, size_y, size_z);
}
```

**2D Shape Rendering:**
```c
// Only apply scaling when scale differs from default
if (s->scale_x != 1.0f || s->scale_y != 1.0f) {
    float scaled_width = s->width * s->scale_x;
    float scaled_height = s->height * s->scale_y;
    // Apply scaled dimensions
    // ...
} else {
    // For backward compatibility - render with original dimensions
    // ...
}
```

#### UI Element System:
- **Hierarchical Layout**: Elements can be nested with parent-child relationships
- **Flexible Positioning**: Supports both absolute and percentage-based sizing
- **Event Handling**: Comprehensive click, keyboard, and mouse event support
- **State Management**: Tracks active elements, selections, and focus states

### Controller (4.controller_v0.01.c)

The Controller handles all user input and event processing.

#### Key Features:
- **Multi-device Input**: Supports mouse, keyboard, and module input
- **Event Propagation**: Handles click events, keyboard input, and UI interactions
- **DOM-like API**: Provides element lookup and manipulation functions
- **Module Communication**: Forwards user input to active modules

#### Input Handling:
- **Mouse Events**: Click detection and position mapping to UI elements
- **Keyboard Events**: Text input for text fields and special keys for modules
- **Arrow Keys**: Forwarded to modules for navigation and movement
- **Modifier Keys**: Ctrl+C/V/X for copy/paste/cut operations

#### Event Processing:
- **Element-specific Events**: Different handling for buttons, textfields, sliders, etc.
- **Dynamic Event Handling**: Can call event handlers by name with dynamic dispatch
- **Module Integration**: Seamlessly forwards events to modules for custom processing

### Main (1.main_prototype_v0.01.c)

The entry point that coordinates the MVC (Model-View-Controller) architecture.

#### Key Features:
- **Application Initialization**: Sets up all components and loads CHTML files
- **Main Loop**: Runs the OpenGL event loop with frame timing
- **Module Management**: Handles module loading, communication, and lifecycle
- **File Navigation**: Supports navigation between CHTML files

## Backward Compatibility Features

### 1. Shape Format Evolution

**Old Format (12 fields):**
```
SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a
e.g., SHAPE;RECT;player;100;100;0;50;50;50;1;0;0;1
```

**New Format (18 fields):**
```
SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a;rot_x;rot_y;rot_z;scale_x;scale_y;scale_z
e.g., SHAPE;RECT;player;100;100;0;50;50;50;1;0;0;1;45.0;0.0;30.0;1.0;1.0;1.0
```

**Compatibility Handling:**
- When 12 fields are detected, rotation values default to 0.0 and scale values to 1.0
- When 18 fields are detected, all values are used as provided
- Rendering only applies transformations when values differ from defaults

### 2. Rendering Backward Compatibility

**Conditional Transformation Logic:**
- Only applies OpenGL transformations when rotation/scale values differ from default
- For old modules with default values, uses identical rendering path as before changes
- Performance is preserved by avoiding unnecessary matrix operations

### 3. Module Protocol Compatibility

**Dual Communication Support:**
- Existing pipe-based modules continue to work unchanged
- New shared memory modules can be added with protocol="shared" attribute
- Mixed module types can coexist in the same CHTML file

## CHTML File Structure and Features

### Basic Syntax
CHTML uses XML-like tags with attributes to define UI elements:

```xml
<window width="800" height="600" color="#000000">
    <canvas x="0" y="0" width="800" height="600" viewMode="3d"/>
    <button x="10" y="10" width="100" height="30" label="Click Me" onClick="handler"/>
    <module>module/+x/my_module.+x</module>
</window>
```

### Supported Elements

**Layout Elements:**
- `window`: Root container with dimensions and colors
- `panel`: Grouping container for other elements
- `canvas`: OpenGL rendering surface for 2D/3D graphics

**Input Elements:**
- `button`: Clickable button with label and event handling
- `textfield`: Single-line text input
- `textarea`: Multi-line text input  
- `checkbox`: Boolean state toggle
- `slider`: Range value selection

**Display Elements:**
- `text`: Static text display with color support
- `link`: Clickable text for navigation
- `dirlist`: Directory listing view

**Navigation Elements:**
- `menu`: Dropdown menu containers
- `menuitem`: Individual menu options
- `<module>`: External module integration

### Module Integration

#### Module Tag Features:
- **Protocol Attribute**: `protocol="shared"` for shared memory, default is pipe-based
- **Path Specification**: Points to compiled module in `module/+x/` directory
- **Bidirectional Communication**: Modules can send SHAPE commands and receive input

## Detailed Analysis of Key CHTML Files

### master-demo]c3.txt

This is a comprehensive demonstration of the CHTML framework's capabilities:

**Key Features Demonstrated:**
- **Complex Layout**: Uses nested panels and precise positioning
- **Multiple UI Elements**: Includes buttons, text fields, text areas, checkboxes, sliders
- **Menu System**: Implements both main menu bar and context menu
- **Canvas Integration**: Includes camera-enabled canvas with `ar="1"` attribute
- **Module Integration**: Uses enhanced_game_ipc.+x for game state
- **Dynamic Text**: Shows player coordinates, score, health in real-time
- **Event Handlers**: Multiple onClick handlers for different actions
- **2D/3D View Switching**: Buttons to switch between view modes

**Module (`enhanced_game_ipc.+x`):**
- Likely handles player movement, scoring, and game state
- Processes arrow key input for blue square movement
- Updates position text, score, health, and status elements
- Communicates with UI via SHAPE commands and variable updates

### test_shared_player_movement.chtml

This file demonstrates advanced 3D visualization and shared memory modules:

**Key Features:**
- **3D Canvas**: Main view with viewMode="3d" for 3D rendering
- **2D Mini Map**: Secondary canvas with viewMode="2d" for overhead view
- **Shared Memory Protocol**: Uses `protocol="shared"` for high-performance communication
- **Dual Canvas Approach**: 3D main view with 2D map for spatial awareness
- **Direct Control Buttons**: Alternative to arrow keys for movement
- **Status Displays**: Real-time position and status updates

**Module (`shared_player_movement.+x`):**
- Implements shared memory communication
- Handles 3D player movement in response to arrow keys
- Updates both 3D view and 2D mini map simultaneously
- Provides position feedback to text elements
- Uses efficient communication for real-time performance

**Backward Compatibility Verification:**
- This file is crucial for testing that new rotation/scale features don't break existing functionality
- The conditional rendering ensures the module continues to work identically

## Module System Deep Dive

### Module Communication Protocol

**SHAPE Commands:**
```
# Old format (12 fields) - still supported
SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a

# New format (18 fields) - with rotation/scale
SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a;rot_x;rot_y;rot_z;scale_x;scale_y;scale_z

# Utility commands
CLEAR_SHAPES                      # Clear all shapes
VAR;label;type;value             # Set module variable
ARRAY;label;type;count;v1,v2,v3  # Set module array
```

**Input Commands:**
- Arrow keys: UP, DOWN, LEFT, RIGHT
- Text input: Direct character input to modules
- Custom commands: GAME_START, JUMP, SHOOT, etc.

### Input Flow Architecture

1. **User Input**: Mouse/keyboard events in Controller
2. **Event Processing**: Controller determines target element and action
3. **Module Communication**: Relevant input sent to appropriate module(s)
4. **Module Processing**: Module processes input and may update state
5. **SHAPE Output**: Module sends SHAPE commands back to Model
6. **State Update**: Model updates Shape data structure
7. **Rendering**: View renders updated shapes using 3D transformations
8. **Display**: OpenGL display shows updated UI

## Advanced Features

### 3D Transformation System

The system now supports full 3D transformations:
- **Rotation**: Around X, Y, and Z axes in degrees
- **Scaling**: Independent scaling on X, Y, and Z axes
- **Translation**: Positioning in 3D space
- **Hierarchical**: Transformations can be combined and nested

### Performance Optimizations

**Conditional Rendering:**
- Only performs expensive transformations when values differ from defaults
- Avoids unnecessary OpenGL matrix operations
- Preserves frame rate for applications without advanced transformations

**Memory Management:**
- Shared memory modules for high-frequency communication
- Efficient shape array management
- Texture caching for emoji rendering

### Camera System

**3D View Controls:**
- Configurable field of view (fov)
- Customizable camera position and target
- Multiple canvas support with different camera settings
- 2D/3D view mode switching

## Security and Stability Features

### Input Validation
- Bounds checking for array access
- String length limitations to prevent buffer overflows
- Module communication sanitization

### Error Handling
- Graceful fallback for missing modules
- Robust parsing that handles malformed input
- Resource cleanup on application exit

### File System Integration
- Safe file access patterns
- Navigation system with proper path validation
- Directory listing with security restrictions

## Development Workflow

### Adding New Features
1. **Modify Model**: Add data structures and parsing logic
2. **Update View**: Add rendering code with backward compatibility
3. **Enhance Controller**: Add input handling if needed
4. **Test Compatibility**: Verify old formats still work
5. **Update Documentation**: Document new features and syntax

### Module Development
1. **Choose Protocol**: Pipe-based (stdin/stdout) or shared memory
2. **Parse Input**: Handle commands from controller
3. **Generate Output**: Send SHAPE commands to update display
4. **Compile**: Use module compilation system to create .+x executable
5. **Test**: Integrate with CHTML and verify functionality

## Maintenance and Future Development

### Current Capabilities
- Complete 2D and 3D rendering with transformations
- Advanced UI with menus, forms, and canvas elements
- Multiple module support with different protocols
- Full backward compatibility with legacy systems
- Real-time interaction and animation support

### Extensibility Points
- New UI element types can be added to View
- Additional communication protocols can be implemented
- New SHAPE command formats can be added while maintaining compatibility
- Camera and lighting systems can be enhanced further

### Best Practices
- Always maintain backward compatibility when adding new features
- Use conditional rendering to avoid performance degradation
- Validate all external inputs for security
- Follow the MVC pattern strictly for maintainability
- Document new syntax elements for future developers

This comprehensive framework demonstrates a sophisticated approach to GUI development using C and OpenGL, with strong emphasis on modularity, performance, and backward compatibility. The recent addition of rotation and scaling features extends its 3D capabilities while preserving all existing functionality.