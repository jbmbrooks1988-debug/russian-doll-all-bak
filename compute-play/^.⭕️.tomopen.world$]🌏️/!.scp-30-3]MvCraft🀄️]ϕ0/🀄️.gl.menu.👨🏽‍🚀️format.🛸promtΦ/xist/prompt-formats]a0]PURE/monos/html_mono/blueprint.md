# HTML Canvas Framework for OpenGL/GLUT Applications - Blueprint

## Overview
This framework allows developers to create 2D/3D games and applications using HTML-like markup and CSS-like styling, with JavaScript-like logic. The framework renders these elements in OpenGL/GLUT contexts, enabling rich UI experiences with game rendering capabilities.

## Architecture: MVC + Modules, Event-Driven, LUT, FSM

### Model
- UI Elements stored as structured data (DOM-like tree)
- Element properties stored in look-up tables (LUT)
- State machines (FSM) for component behavior
- Data persistence through CSV files for inter-module communication

### View
- OpenGL renderer that interprets HTML-like markup
- Coordinate system supporting both absolute and relative positioning
- Canvas for 2D/3D rendering with UI overlay capabilities

### Controller
- Event handler for keyboard/mouse input
- Markup parser to convert HTML-like syntax to OpenGL render calls
- Module communication via system() calls and pipes

## Key Features

### 1. HTML-like Markup
```html
<div class="header" pos="0,0,800,50" bg="#333333">
  <button id="menu_btn" pos="10,10,100,30" bg="#666666" onclick="show_menu()">Menu</button>
</div>
<canvas id="game_canvas" pos="0,50,800,550" />
```

### 2. CSS-like Styling
- Positioning: `pos="x,y,width,height"` or `pos="left:10;top:10;width:100;height:30"`
- Colors: `bg="#RRGGBB"`, `fg="#RRGGBB"`, `border="#RRGGBB"`
- Text: `font="Arial"`, `size="12"`, `align="center"`

### 3. JavaScript-like Logic
- Event handlers: `onclick="function_name()"`, `onhover="function_name()"`
- Element manipulation via ID access
- Simple control flow (if/else, loops) through markup attributes

### 4. Module Architecture
- Core renderer as primary binary
- External executables for complex logic (physics, AI, etc.)
- Communication via system() calls and pipes
- Data sharing through CSV files

## Core Components

### 1. UI Elements
- Structural: div, header, footer, aside, section
- Interactive: button, input, select, textarea, checkbox, radio
- Canvas: 2D/3D rendering context within HTML structure
- Menus: dropdown, context menu, toolbar

### 2. Event System
- Mouse events: click, hover, drag, scroll
- Keyboard events: keydown, keyup, keypress
- Custom events: state changes, data updates

### 3. Rendering Pipeline
1. Parse HTML-like markup into element tree
2. Apply CSS-like styles to elements
3. Calculate positions (absolute/relative)
4. Handle event binding
5. Render elements using OpenGL calls
6. Process events and update state
7. Repeat (60+ FPS)

### 4. Coordinate System
- Absolute coordinates: `pos="x,y,width,height"`
- Relative coordinates: `pos="left:10%;top:5%;width:80%;height:90%"`
- Support for pixel and percentage units
- Transformations (rotation, scale) via attributes

## Implementation Details

### Element Structure
```c
typedef struct {
    char id[64];
    char tag[32];
    float pos[4]; // x, y, width, height
    float bg_color[4]; // r, g, b, a
    float fg_color[4]; 
    char text[256];
    void (*onclick_handler)();
    void (*onhover_handler)();
    struct element_t *children;
    int child_count;
    int visible;
    int enabled;
} element_t;
```

### Parser
- Tokenize HTML-like markup
- Create element tree
- Apply attributes to elements
- Bind event handlers

### Renderer
- Traverse element tree
- Convert positions to OpenGL coordinates
- Render elements using appropriate OpenGL functions
- Handle canvas rendering within UI elements

### Event Handler
- Capture input events
- Map events to elements based on position
- Trigger appropriate handlers
- Update element states

## Module Integration

### External Logic Modules
```c
// Call external module with data
int call_external_module(const char* module_name, const char* input_data, char* output_buffer) {
    char command[512];
    snprintf(command, sizeof(command), "./%s", module_name);
    
    FILE* pipe = popen(command, "w");
    if (!pipe) return -1;
    
    fprintf(pipe, "%s", input_data);
    pclose(pipe);
    
    // Read result from file or pipe
    return 0;
}
```

### Data Sharing via CSV
- Store game state, UI state, and other data in CSV files
- Enable communication between different modules
- Allow persistent data across application runs

## Rendering Elements

### Basic Elements
- **div**: Container element
- **button**: Clickable element with text
- **input**: Text input field
- **canvas**: Area for 2D/3D rendering

### Positioning System
- Support for absolute positioning: `pos="10,20,100,50"`
- Support for relative positioning: `pos="left:5%;top:10%;width:90%;height:80%"`
- Z-index for layering elements

### Styling
- Background colors: `bg="#RRGGBB"`
- Border styles: `border="#RRGGBB;width:2px"`
- Text properties: `color="#FFFFFF";font_size="14px"`

## Event System

### Event Binding
- `onclick`: Function called when element is clicked
- `onhover`: Function called when mouse hovers over element
- `onchange`: Function called when element value changes

### Event Propagation
- Bubble events from child to parent elements
- Allow event cancellation

## Canvas Integration

### 2D/3D Rendering
- Embed canvas elements within HTML structure
- Allow canvas to receive mouse/keyboard events
- Support for custom rendering within canvas bounds

### Mixed Content
- Render UI elements overlaid on game content
- Support for game elements within UI containers

## File Structure
- `main.c` - Entry point with basic GLUT setup
- `parser.c` - HTML-like markup parser
- `renderer.c` - OpenGL rendering functions
- `events.c` - Event handling system
- `elements.c` - UI element definitions and methods
- `modules.c` - External module communication
- `styles.c` - Styling and CSS-like properties
- `markup.html` - UI definition file
- `data.csv` - Shared data file

## Performance Considerations
- Efficient element tree traversal
- Batch rendering where possible
- Minimize OpenGL state changes
- Cache computed positions/styles
- Lazy evaluation of invisible elements

## Extensibility
- Plugin architecture for custom elements
- Scripting interface for complex logic
- Theme support for styling
- Localization support for text