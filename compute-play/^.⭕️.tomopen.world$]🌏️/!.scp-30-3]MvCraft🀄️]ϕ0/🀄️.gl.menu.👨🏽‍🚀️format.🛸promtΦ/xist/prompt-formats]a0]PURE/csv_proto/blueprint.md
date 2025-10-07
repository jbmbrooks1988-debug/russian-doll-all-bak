# Framework Blueprint: CSV-Based UI Framework for OpenGL/Glut

## Overview
This framework aims to create a declarative UI system using CSV files that can interpret and render UI elements, game elements, and interactive components. The goal is to separate the visual/interactive logic from the C code, making it easier for AI tools to understand and modify.

## Architecture: MVC + Modules
The system follows an MVC pattern with additional module-based separation:

### Model (model.c)
- Parses and maintains state for CSV-based UI definitions
- Manages UI elements and their properties (position, size, type, etc.)
- Handles data structures for UI elements and game objects
- Maintains element state (hovered, clicked, active, etc.)

### View (view.c)
- Renders UI elements based on model state
- Handles OpenGL drawing of elements
- Implements drawing primitives for various UI types (buttons, sliders, etc.)
- Manages coordinate systems (relative/absolute)

### Controller (controller.c)
- Handles input events (mouse, keyboard)
- Processes UI interactions and callbacks
- Updates model based on user input
- Manages event dispatch system

## UI Element Types
The framework supports various element types:

### Structural Elements
- Header bars
- Side panels
- Grid containers
- Window frames

### Reactive Elements
- Buttons
- Toggle switches
- Sliders/thumbs
- Checkboxes
- Radio buttons

### Peripherals
- File explorers
- Context menus
- Toolbars
- Status bars

### Canvas Elements
- Game rendering areas
- 2D graphic canvases
- 3D object viewers
- Drawing surfaces

## CSV Markup Format

The UI is defined using CSV files with the following structure:

```csv
element_type,id,parent_id,x,y,width,height,relative_x,relative_y,text,onclick_callback,properties
button,btn_start,root,0,0,100,30,0.5,0.5,"Start Game","start_game",visible=1;enabled=1
slider,sldr_volume,root,10,50,200,20,0.1,0.3,"","volume_changed","min=0;max=100;value=50"
canvas,game_canvas,root,0,0,800,600,0,0,"","","width=800;height=600;type=game"
```

### Column Definitions:
- `element_type`: Type of UI element (button, slider, canvas, etc.)
- `id`: Unique identifier for the element
- `parent_id`: Parent container for this element
- `x,y`: Absolute position (0 if using relative)
- `width,height`: Size of element
- `relative_x,relative_y`: Relative position (0-1, 0-1)
- `text`: Text content for labels/buttons
- `onclick_callback`: Function name to call on interaction
- `properties`: Additional properties in key=value;key=value format

## Coordinate System
- Supports both absolute (pixel) and relative (percentage/0-1 scale) coordinates
- Relative positioning helps with responsive design
- Mixed coordinate systems are possible

## Module System
- Separate executables for non-OpenGL logic
- Communication via pipes/system() calls
- Keeps rendering thread lightweight
- Modules can be written in other languages with same interpreter

## Event System
- Event-driven architecture
- Look-up tables (LUT) for event handling
- Finite State Machines (FSM) for complex interactions
- Callback system for user-defined actions

## Input Handling
- Mouse input (click, hover, drag, scroll)
- Keyboard input (keypress, text input)
- Touch input (future expansion)
- Game controller input (future expansion)

## Rendering Pipeline
1. Parse CSV definition
2. Build element tree in model
3. Receive input events in controller
4. Update model state
5. Render elements in view
6. Handle callbacks
7. Repeat

## File Structure
```
project/
├── main.c (or main_prototype_0.c) - OpenGL context management
├── model.c - Data structures and state management
├── view.c - Rendering and drawing functions
├── controller.c - Input handling and event processing
├── ui_definitions/
│   ├── main_menu.csv
│   ├── game_ui.csv
│   └── settings.csv
├── modules/
│   ├── file_explorer.exe
│   └── asset_loader.exe
└── callbacks.c - User-defined callback functions
```

## Communication Protocol
For external modules:
- Use pipes for real-time communication
- Use CSV files for data sharing between modules
- Use system() calls to spawn/communicate with modules
- Maintain thin MVC layer for performance

## Implementation Considerations
- Memory management for dynamic UI elements
- Efficient rendering (batch drawing where possible)
- Support for custom shaders
- Animation support
- Texture loading for UI elements
- Performance profiling for large UIs

## Security Considerations
- Input validation for CSV files
- Safe callback execution
- Sandbox external modules if needed

This blueprint provides the foundation for a flexible, declarative UI system that can be easily modified and understood by both humans and AI tools.