# CHTML Framework Capabilities for Paint-like Editor

## Current Architecture Overview

The CHTML framework follows an MVC+ (Model-View-Controller-Game Module) architecture:

- **Model (model.c)**: Manages data and logic, communicates with game modules via pipes, holds game state
- **View (view.c)**: Handles rendering and UI display using OpenGL
- **Controller (controller.c)**: Processes user input and forwards to model
- **Game Module (game_ipc.c)**: Contains application logic in separate executable

## UI Elements Available

### Core Elements
- **Canvas**: 2D/3D rendering area with view mode switching
- **Button**: Clickable UI elements with event handling
- **Panel**: Container for other elements
- **Header**: Top bar elements
- **Text**: Static text display
- **Window**: Root application container

### Current Limitations
- No built-in color picker component
- No grid system for structured placement
- Limited event handling for complex UI interactions

## Required Capabilities for Paint-like Editor

### 1. Color Picker Sidebar

**Current State:**
- ✗ No dedicated color picker element exists
- ✗ No sidebar container element specifically designed for pickers

**Framework-level additions (CHTML MVC code):**
- Add color picker UI element type with 8x 42x42 swatches arranged in grid
- Create vertical sidebar layout
- Implement swatch rendering in view.c
- Add mouse interaction handling in controller.c
- Store selected swatch state in UIElement

**Application logic (game_ipc.c):**
- Define what goes in each swatch (colors, images, brushes, etc.)
- Track currently selected swatch
- Handle swatch selection communication via pipe protocol

**Implementation Path:**
- Add new UI element type: `colorpicker` (framework)
- Add swatch grid elements in parser (framework)
- Implement rendering logic for swatches in view.c (framework)
- Add mouse handling for swatch selection in controller.c (framework)
- Communicate swatch selection to game module via pipe protocol (both)
- Game module determines swatch contents and meaning (application)

### 2. Top Header Bar with Mode Switching

**Current State:**
- ✓ Canvas has 2D/3D switching capability (already implemented)
- ✓ Header element exists for top bar elements
- ✗ Mode switcher buttons are not in header bar

**Framework-level additions (CHTML MVC code):**
- Move 2D/3D buttons to header element in UI layout
- Ensure proper rendering order (header always visible)

**Application logic (game_ipc.c):**
- Handle 2D/3D mode switching state

**Implementation Path:**
- Reposition buttons in CHTML file (framework)
- Update canvas rendering to respect mode in canvas_render_sample (framework)
- Update game module to handle mode switching (application)

### 3. Grid-based Canvas System

**Current State:**
- ✗ No grid system exists
- ✓ Canvas supports both 2D and 3D rendering
- ✓ Shape primitives available (SQUARE, CIRCLE)

**Framework-level additions (CHTML MVC code):**
- Grid overlay rendering in canvas_render_sample function
- Mouse coordinate to grid coordinate mapping in controller
- Grid visualization rendering in view.c
- Click detection and coordinate mapping
- Grid interaction area detection

**Application logic (game_ipc.c):**
- Block placement and removal logic at grid coordinates
- Block persistence between sessions
- Grid snapping functionality
- What to place when a user clicks on canvas (block, brush stroke, etc.)
- How swatch selection affects placement

**Implementation Path:**
- Add grid rendering to canvas_render_sample (framework)
- Add coordinate translation in controller.c (framework)
- Implement grid interaction logic in game module (application)
- Framework detects clicks and sends coordinates to game module (framework)
- Game module interprets click context and responds with shape updates (application)

## Implementation Architecture

### Model Component (model.c) - Framework
- Forward user input to game module via pipe
- Receive and maintain shape data from game module
- Manage pipe communication with game module

### View Component (view.c) - Framework
- Render canvas with grid overlay
- Render color picker sidebar UI
- Render header with mode switching buttons
- Update canvas based on shape data from model

### Controller Component (controller.c) - Framework
- Handle mouse clicks on swatches for selection
- Handle canvas clicks and map coordinates to grid
- Detect click context (canvas vs. color picker)
- Forward user actions with coordinates to model

### Game Module (game_ipc.c) - Application Logic
- Process grid-based placement logic
- Manage swatch selection meaning
- Handle application-specific game rules
- Generate shape data for rendering based on grid state

## Technical Considerations

### Grid System
- Coordinate transformation from screen space to grid space (framework responsibility)
- Grid snapping accuracy for placement (application responsibility)
- Performance optimization for grid rendering (framework responsibility)

### Swatch System
- Click detection and handling (framework responsibility)
- Swatch content meaning and persistence (application responsibility)

### Communication Protocol
- Format for swatch selection events: `SWATCH_SELECTED 0-7`
- Format for canvas placement: `PLACE x,y,swatch_index`
- Format for block removal: `REMOVE x,y`

## Clear Separation of Responsibilities

### Framework-level (CHTML MVC code):
- UI rendering and layout
- Basic mouse/keyboard input handling
- Canvas rendering with grid overlay
- Swatch UI rendering and selection
- Communication via pipes
- Coordinate system transformations (screen to grid)
- Click detection and coordinate mapping

### Application Logic (game_ipc.c):
- What content goes in swatches (colors, brushes, tools, etc.)
- How to interpret swatch selection
- Grid-based placement algorithms
- Placement meaning and persistence
- Game state management
- Grid interaction business logic

## Conclusion

The CHTML framework architecture correctly separates framework concerns from application logic. The UI framework components (click detection, coordinate mapping, swatch rendering) are implemented in the MVC code, while the application-specific behavior (what gets placed, how swatches are interpreted, placement rules) belongs in the game module. The communication between them happens via the established pipe protocol.

The framework provides the infrastructure for detecting user interactions, while the game module defines the meaning and outcome of those interactions.