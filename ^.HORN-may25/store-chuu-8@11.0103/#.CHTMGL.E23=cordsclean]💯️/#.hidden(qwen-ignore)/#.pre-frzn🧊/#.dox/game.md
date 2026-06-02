# Game Implementation Plan

## Overview
We'll implement a scalable module platform where the second argument specifies which module executable to run. This allows the system to support different types of applications (games, utilities, etc.) not just one specific type. The platform will be able to run various modules that can interact with the canvas, with the first module being a simple game where the user controls a blue square with arrow keys.

## Architecture

### Core Components
1. **Canvas Element**: Interactive surface that modules can draw to
2. **Module Manager**: Handles execution of different module executables 
3. **Input Handler**: Routes user input to active module
4. **Renderer**: Updates canvas to show module state

### Module System Structure
The main application accepts two arguments:
- `./main_prototype_0 markup_file.chtml [module_executable]`

The module executable is dynamically executed based on the second argument:
```
@module/
├── module_example.c (existing)
├── game/
│   ├── game.c (game logic with canvas interaction)
│   └── Makefile
├── other_modules/
│   ├── ...
```

### Data Flow
1. User input (arrow keys, mouse, etc.) → controller.c detects event → sends to active module
2. Active module executable processes input and updates state  
3. Updated state is shared via CSV files (input.csv and output.csv)
4. Main application reads output.csv and updates canvas rendering
5. This creates a platform approach where different modules can be swapped in/out

### Current Status & Next Steps
- Module execution system: IMPLEMENTED
- Arrow key handling: IMPLEMENTED  
- Basic game module: CREATED
- Blue square rendering on canvas: NOT YET IMPLEMENTED
- Debugging information for arrow keys and player coordinates: NEEDED

### Scalability Features
- Any compatible module executable can be passed as second argument
- Modules follow a standard interface using CSV for input/output
- Canvas rendering allows for visual modules (games, drawing, etc.)
- Framework can support both interactive and batch-style modules
- Data sharing via CSV files allows for language-agnostic modules

## Implementation Details

### 1. Module Interface
The game module will work similarly to the existing module:
- Input: Current position (x, y), input commands, canvas dimensions (via CSV or other format)
- Output: New position (x, y) after processing input

### 2. Game State
Game state to be maintained:
- Player position (x, y coordinates in canvas space)
- Canvas boundaries (width, height)
- Player velocity (for smooth movement if desired)
- Game objects (initially just the blue square)

### 3. Integration Points
- **main_prototype_0.c**: Initialize game module
- **controller.c**: Capture arrow key events and call game logic
- **view.c**: Render blue square in canvas based on game state
- **demo.chtml**: Define canvas element with appropriate attributes

### 4. File Structure
```
@module/game/
├── game.c          # Main game logic (movement, collision)
├── game_state.h    # Game state structures
├── input_handler.c # Process input commands
└── Makefile        # Build game module
```

## Technical Implementation

### Game Module Interface
The game module will be callable from the main application, similar to how `run_module_handler()` works:
```c
// Input files:
// input.csv: player_x,player_y,input_command,canvas_width,canvas_height
// Output files:
// output.csv: new_player_x,new_player_y
```

### Canvas Integration
- The canvas element in CHTML will have a special `game` attribute or `id` to identify it as the game canvas
- The canvas rendering function will draw the game state (blue square) on top of any other canvas content
- The game state will be stored globally in the view or model

### Input Handling
- Arrow keys will be captured in `controller.c`'s `special_keys()` function
- Arrow key input will trigger a call to the game module
- Game module will update the player position based on the input and boundaries

## Expected User Experience
1. User opens application with a CHTML file containing a game canvas
2. User presses arrow keys to control the blue square within the canvas
3. Blue square moves smoothly and stays within canvas boundaries
4. Other UI elements continue to function normally during gameplay

## Dependencies
- Existing MVC architecture
- Canvas rendering capabilities in view.c
- Input handling in controller.c
- GLUT for graphics and input (already present)

This architecture maintains the existing design patterns while extending the framework to support interactive games.

