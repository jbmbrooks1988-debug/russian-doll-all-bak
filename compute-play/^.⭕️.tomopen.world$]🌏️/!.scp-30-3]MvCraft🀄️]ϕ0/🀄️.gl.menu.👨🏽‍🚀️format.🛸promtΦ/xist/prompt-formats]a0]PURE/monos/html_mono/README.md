# HTML Canvas Framework for OpenGL/GLUT Applications

This framework allows you to create 2D/3D games and applications using HTML-like markup with OpenGL/GLUT rendering.

## Features

- HTML-like UI elements (div, button, canvas)
- Mouse interaction (click, hover)
- Basic 2D rendering in canvas
- Simple 3D rendering example
- Relative positioning system
- External module integration
- CSV data sharing between modules
- Multi-screen UI support
- Animated game content

## Building and Running

### Prerequisites

- OpenGL development libraries
- GLUT development libraries
- GCC compiler
- Math library (libm)

On Ubuntu/Debian:
```bash
sudo apt-get install libgl1-mesa-dev libglu1-mesa-dev freeglut3-dev
```

On CentOS/RHEL:
```bash
sudo yum install mesa-libGL-devel mesa-libGLU-devel freeglut-devel
```

### Compile

Main prototype:
```bash
gcc -o main_prototype_0 main_prototype_0.c -lGL -lGLU -lglut
```

Demo project:
```bash
gcc -o demo_project demo_project.c -lGL -lGLU -lglut -lm
```

Or using the Makefile:
```bash
make
```

### Run

Main prototype:
```bash
./main_prototype_0
```

Main prototype with custom markup:
```bash
./main_prototype_0 demo_ui.html
```

Demo project:
```bash
./demo_project
```

## Project Structure

- `main_prototype_0.c` - Core framework implementation
- `demo_project.c` - Complete demo with multi-screen game interface
- `demo_ui.html` - Example markup file
- `game_logic.sh` - External module example
- `blueprint.md` - Architecture document
- `chtml_legend.md` - Feature reference
- `progress.md` - Development progress
- `README.md` - This file
- `Makefile` - Build automation
- `backup/` - Backup archives
- `backup_script.sh` - Automated backup utility

## Markup Format

The framework uses a simplified HTML-like syntax:

```html
<div id="element_id" pos="x,y,width,height" bg="#RRGGBB" fg="#RRGGBB" text="content" onclick="handler_function" />
```

- `pos`: Position and size in format "x,y,width,height"
- `bg`: Background color in hex format
- `fg`: Foreground (text) color in hex format
- `text`: Text content for elements that display text
- `onclick`: Name of function to call when element is clicked

## Architecture

- MVC + Modules design pattern
- Event-driven system
- Look-up tables (LUT) for element properties
- Finite State Machines (FSM) for component behavior
- No new header files (all in single .c file for simplicity)

## Demo Features

The demo project (demo_project.c) shows:
- Multi-screen interface (Main Menu, Game Screen, Settings)
- Interactive buttons with hover effects
- Animated game content with moving player/enemies
- Health/Mana indicators
- Sidebar UI with game controls
- Screen switching functionality
- Dynamic content rendering

## Framework Capabilities

The CHTML framework provides:

### UI Elements
- Structural: div containers
- Interactive: buttons
- Rendering: canvas areas

### Positioning & Styling
- Absolute positioning with `pos="x,y,width,height"`
- Color styling with `bg="#RRGGBB"` and `fg="#RRGGBB"`
- Text content with `text="content"`

### Events & Interaction
- Mouse click and hover detection
- Element-specific event handlers
- Coordinate mapping between screen and elements

### Rendering
- OpenGL-based rendering pipeline
- Support for both 2D and 3D rendering in canvas
- Batch rendering of UI elements

### Module Integration
- External binary execution
- Data sharing through CSV files
- Process communication via standard I/O