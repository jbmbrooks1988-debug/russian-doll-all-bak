# MVC Framework Changes for Vertical Slider and Canvas Updates

This document details the changes made to the CHTML MVC framework to add vertical slider functionality and enable canvas updates with bidirectional communication.

## Overview
The original framework only supported horizontal sliders. The modifications enabled:
- Vertical slider support based on aspect ratio (height > width)
- Real-time canvas updates via external modules
- Bidirectional communication between UI and modules

## Changes Made

### 1. View Layer (3.view.c)

#### Vertical Slider Rendering
- **Detection Method**: Sliders are identified as vertical when `height > width`
- **Track Drawing**: Added vertical track rendering with centered position
- **Thumb Positioning**: Implemented proper thumb placement based on normalized value
- **Coordinate System**: Used proper OpenGL coordinate system for vertical orientation

#### Key Code Addition:
```c
int is_vertical = (el->height > el->width);
if (is_vertical) {
    // Draw vertical slider
    float normalized_pos = (float)(el->slider_value - el->slider_min) / (el->slider_max - el->slider_min);
    float thumb_pos_y = abs_y + normalized_pos * (el->height - 10);
    // ... render thumb at calculated position
}
```

### 2. Controller Layer (4.controller.c)

#### Mouse Interaction Handling
- **Click Detection**: Modified to handle both horizontal and vertical sliders
- **Coordinate Conversion**: Added proper coordinate transformation for vertical sliders
- **Value Calculation**: Implemented normalized value calculation for vertical orientation

#### Key Changes:

**In mouse callback function:**
- Added aspect ratio check for vertical sliders
- Implemented proper Y-coordinate conversion using existing `convert_y_to_opengl_coords` function
- Fixed coordinate system to match OpenGL (bottom = 0) vs Window (top = 0)

**Slider Value Updates:**
- Added communication to send slider changes to the module using `model_send_input`
- Format: `SLIDER:<id>:<value>` for each slider movement

**Canvas Click Handling:**
- Added canvas click detection with relative coordinate calculation
- Removed absolute coordinates, now sending coordinates relative to canvas origin
- Added boundary checking to ensure clicks are within canvas area
- Format: `CANVAS:<rel_x>,<rel_y>` for each canvas click

### 3. Model Layer (2.model.c)

#### IPC Communication
- The existing model infrastructure already supported bidirectional communication
- Used existing `model_send_input` and `update_model` functions
- Module communication uses the enhanced format: `SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a`
- Variable updates use: `VAR;label;type;value` format

### Module Communication Protocol

#### Input to Module:
- `SLIDER:<id>:<value>` - When slider values change
- `CANVAS:<x>,<y>` - When canvas is clicked (relative coordinates)

#### Output from Module:
- `CLEAR_SHAPES` - Clear previous shapes
- `SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a` - Render shapes
- `VAR;label;type;value` - Update UI variables
- `ARRAY;label;type;count;val1,val2,...` - Send arrays to UI

## Technical Implementation Details

### Coordinate System Handling
- Window coordinates: (0,0) at top-left, Y increases downward
- OpenGL coordinates: (0,0) at bottom-left, Y increases upward
- Canvas relative coordinates: (0,0) at bottom-left of canvas, Y increases upward

### Slider Value Calculation
- Horizontal: `normalized_value = (mouse_x - slider_left) / slider_width`
- Vertical: `normalized_value = (mouse_y - slider_bottom) / slider_height`

### Canvas Communication
- Canvas clicks are sent with coordinates relative to the canvas origin
- This allows modules to properly interpret where the click occurred
- Boundary checks ensure clicks are only processed when actually within canvas area

## Integration Points

### UI-to-Module Communication
- Sliders send value changes via `model_send_input` when moved
- Canvas clicks send coordinates via `model_send_input` when clicked
- All communication happens asynchronously through IPC pipes

### Module-to-UI Updates
- Modules continuously output shape and variable updates
- UI automatically updates based on received messages
- `update_ui_with_model_variables` function maps module variables to UI elements

## Key Design Principles

### Backward Compatibility
- Horizontal sliders continue to work as before
- Vertical sliders are detected automatically by aspect ratio
- Existing modules continue to function without changes

### Coordinate Consistency
- All canvas coordinates are relative to canvas origin
- Slider calculations use consistent coordinate systems
- Coordinate transformations are handled at the controller level

### Performance Considerations
- Continuous module updates provide smooth animation
- Coordinate calculations are optimized for real-time interaction
- Efficient shape rendering with OpenGL