# CSV-Based UI Framework for OpenGL/Glut

This project implements a declarative UI system using CSV files that can interpret and render UI elements, game elements, and interactive components for OpenGL/Glut applications.

## Architecture

The system follows an MVC pattern with additional module-based separation:

- **Model (model.c)**: Parses and maintains state for CSV-based UI definitions
- **View (view.c)**: Renders UI elements based on model state
- **Controller (controller.c)**: Handles input events and processes UI interactions

## Features

- Declarative UI definition using CSV files
- Support for various element types (buttons, sliders, containers, etc.)
- Relative and absolute positioning system
- Event-driven architecture
- Callback system for user-defined actions
- Support for game and menu UIs

## File Structure

```
csv_proto/
├── main_prototype_0.c     - OpenGL context manager
├── model.c                - Data structures and state management
├── view.c                 - Rendering and drawing functions
├── controller.c           - Input handling and event processing
├── ui_definitions/        - CSV files defining UI layouts
│   ├── main_menu.csv
│   └── game_menu.csv
├── Makefile               - Build configuration
└── blueprint.md           - Architecture documentation
```

## Building

To build the project:

```bash
make
```

To run the demo:

```bash
make run
# or
./csv_ui_framework
```

## CSV Format

UI elements are defined in CSV files with the following columns:

```
element_type,id,parent_id,x,y,width,height,relative_x,relative_y,text,onclick_callback,properties
```

Example:
```csv
button,btn_start,root,0,0,150,50,0.5,0.3,"Start Game","start_game","color=0.2;0.6;0.8;1.0"
slider,sldr_volume,root,200,300,300,30,0.25,0.25,"Volume","volume_changed","min=0;max=100;value=75"
```

## Element Types

- `button` - Clickable buttons
- `slider` - Slidable controls
- `canvas` - Game/rendering areas
- `label` - Text elements
- `container` - Layout containers
- `image` - Image elements

## Controls

- ESC: Exit the application
- Mouse: Interact with UI elements
- Drag sliders to change values

## Module System

Non-OpenGL logic should run as separate executables using system() calls or pipes to keep the MVC components lightweight.

## Customization

To create your own UI, modify or create new CSV files in the `ui_definitions/` directory and update the filename in `main_prototype_0.c`.