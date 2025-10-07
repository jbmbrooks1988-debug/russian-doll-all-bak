# CHTML (C-HTML) Framework Legend

This document details all available features, elements, attributes, and functionality in the CHTML framework for OpenGL/GLUT applications.

## Core Elements

### Structural Elements
- `div`: Generic container element
  - Usage: `<div id="element_id" pos="x,y,width,height" bg="#RRGGBB" />`

### Interactive Elements
- `button`: Clickable button element
  - Usage: `<button id="btn_id" pos="x,y,width,height" bg="#RRGGBB" text="Button Text" onclick="function_name" />`

### Rendering Elements
- `canvas`: Area for 2D/3D rendering
  - Usage: `<canvas id="canvas_id" pos="x,y,width,height" bg="#RRGGBB" />`

## Positioning System

### Coordinate Format
- Absolute: `pos="x,y,width,height"` - Pixel values from top-left corner
- Example: `pos="10,20,100,50"` places element at (10,20) with width 100 and height 50

### Positioning Attributes
- `x`: Horizontal position from left
- `y`: Vertical position from top
- `width`: Element width
- `height`: Element height

## Styling Attributes

### Color Attributes
- `bg`: Background color in hex format (e.g., `bg="#RRGGBB"`)
- `fg`: Foreground/color for text in hex format (e.g., `fg="#RRGGBB"`)

### Text Attributes
- `text`: Text content to display on the element
- Example: `text="Hello World"`

## Event Attributes

### Available Events
- `onclick`: Function to call when element is clicked
  - Example: `onclick="function_name"`
- `onhover`: Function to call when mouse hovers over element
  - Example: `onhover="function_name"`

## Core Framework Capabilities

### Rendering
- OpenGL-based rendering of UI elements
- Support for both 2D and 3D rendering within canvas elements
- Color fills, borders, and basic shapes
- Text rendering capabilities

### Event Handling
- Mouse click detection
- Mouse hover detection
- Coordinate mapping from screen to element space
- Event propagation system

### Element Management
- Hierarchical element structure (parent-child relationships)
- Element identification by ID
- Show/hide functionality
- Enable/disable state management

### External Module Integration
- System call execution for external binaries
- Data passing to external modules
- Output capture from external modules

### Data Persistence
- CSV-based data sharing between modules
- Simple data serialization/deserialization
- Cross-module state sharing

## Coordinate System

The framework uses a screen coordinate system:
- (0,0) is at the top-left corner
- X increases to the right
- Y increases downward
- All coordinates are in pixels

## Color System

Colors are specified in hexadecimal RGB format:
- Format: `#RRGGBB` where RR=red, GG=green, BB=blue
- Values range from 00 to FF (0 to 255 in decimal)
- Example: `#FF0000` = red, `#00FF00` = green, `#0000FF` = blue

## Module Communication

External modules can be executed using system calls:
- Modules are separate executable files
- Communication through standard input/output
- Data can be passed as command-line arguments or via standard input
- Results can be captured from standard output

## Canvas Integration

The canvas element serves as a special area where:
- 2D and 3D OpenGL rendering can occur
- Game content can be displayed
- Custom rendering code can execute
- UI elements can overlay the game content

## Screen Management

The framework supports multiple screens/states:
- Visibility toggling for different UI sets
- State management between different screen types (menu, game, settings)
- Dynamic content switching based on application state

## Animation & Dynamic Content

- Time-based updates for animations
- Game loop integration
- Dynamic element positioning
- Procedural content generation

## Current Limitations

- No advanced layout systems (flexbox, grid)
- No text input fields in this version
- No image loading capabilities
- No animation support (in basic version)
- No CSS styling (only inline attributes)
- Single-threaded operation
- No built-in theming system

## Planned Additions

Future versions will include:
- More UI elements (input, select, checkboxes, etc.)
- CSS-like stylesheets
- Advanced event handling
- Animation capabilities
- Image support
- Theming system
- Accessibility features

## Example Usage

### Basic UI Structure:
```
<div id="main_container" pos="0,0,800,600" bg="#222222">
  <div id="header" pos="0,0,800,60" bg="#333333">
    <button id="menu_btn" pos="10,10,100,40" bg="#5555aa" fg="#ffffff" text="Menu" onclick="show_menu" />
    <button id="exit_btn" pos="700,10,90,40" bg="#aa5555" fg="#ffffff" text="Exit" onclick="exit_app" />
  </div>
  <canvas id="game_canvas" pos="0,60,800,540" bg="#111111" />
</div>
```

### Multi-Screen Structure (from demo project):
```
<div id="root_container" pos="0,0,1024,768" bg="#1a1a1a">
  <!-- Main Menu Screen -->
  <div id="main_menu" pos="0,0,1024,768" bg="#222233">
    <button id="start_btn" pos="462,250,150,50" bg="#4CAF50" text="START GAME" />
    <button id="settings_btn" pos="462,320,150,50" bg="#64B5F6" text="SETTINGS" />
    <button id="exit_btn" pos="462,390,150,50" bg="#EF5350" text="EXIT" />
  </div>
  
  <!-- Game Screen -->
  <div id="game_screen" pos="0,0,1024,768" bg="#000000" visible="false">
    <canvas id="game_canvas" pos="0,0,824,768" bg="#0f0f14" />
    <div id="game_sidebar" pos="824,0,200,768" bg="#222233">
      <div id="health_bg" pos="844,20,160,30" bg="#4d0000" />
      <div id="health_fill" pos="844,20,120,30" bg="#cc3333" />
      <button id="game_back_btn" pos="844,708,160,40" bg="#7e57c2" text="BACK TO MENU" />
    </div>
  </div>
</div>
```

This defines a multi-screen interface with different UI sets for menu and game states.