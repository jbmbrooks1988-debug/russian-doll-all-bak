{ppost? reprompt from scratch . read maps . just do rmmvc peice by peice, 
however having stuff for this game acutally doesnt hurt esp with a 3d switch . 
then u can do ur mc and add rmmvc

if i do fork 2 hardcode game in "empty model.c instead"
then it would be an 'informed fork'(it is pretty weird that  model is empty...

i kinda feel like we should have game related data...
including abilities 2 load maps and peices...since all games do that, but 
w/e 
<view could communicate 2 model for changing vars , even thru a piped
game module manipulating primatives and could be player.c ; ofc
that means current module system is 'flawed' it was always weird...
'it was ill thought out and hap hazard tbh>

just read over the code more or dont expect it 2 go 'ur way'
👨🏽‍🚀️
i may explain it my goals and ask about the arch"
i assume module.+x isn't doing much so i may just nuke it and ask from 
that pov + data is supposed to be handled in model.c i know that. 
so it needn't "leave mem' save for sub pipe for w/e game its launching, 
but this is more informed and aware than before.

tldr : prompt better but use this foundation 'if u want' it has plenty going for it otherwise. 


also still dont know how update is working (probably csv)
doesn't have to be tho, could be pipe.
👨🏽‍🚀️

💬️
1.model 2.view 3.controller
this is chtml. current its good at rendering .chtml but has no code for logic/dataflow model.c will contain some boiler plate primatives then run apps as "binaries" using pipe() to send and take data from binary passed as arg2  ;

(currently it as a system to run modules/<name>.c as arg to and read write data using
io.csv's but that was poorly architected. 
in step1 we want to remove that old functionality. 
.
the functionality will be simliar but will run module/<name>.c
inside model.c and read(game logic) and write(controler input) to it via pipes

in model.c we are going to define primatives that a game may contain. 
that need to be communicated to view , or chtml elements
such as pixels,circles,squares, spheres, cubes etc (
remove the reading from external .csv  .
any data in put will be done inside model.c 
💬️

}
# Bomberman Game Architecture Blueprint for C-HTML Framework

## Overview
This document outlines the architecture for implementing a Bomberman game using the C-HTML MVC framework. The game will follow the existing pattern of using external modules for game logic while leveraging the C-HTML UI system for rendering and user interaction.

## Current Architecture Analysis

### MVC Structure
The current C-HTML framework follows a Model-View-Controller pattern:

- **Model (2.model.c)**: Currently just a placeholder, but can be extended
- **View (3.view.c)**: Handles UI rendering including canvas elements with custom render functions
- **Controller (4.controller.c)**: Handles user input, module execution, and event handling
- **Main (1.main_prototype_a1.c)**: Initializes the MVC components and sets up GLUT callbacks

### Module System
The framework supports external modules that:
- Receive input via `input.csv` in the format: `player_x,player_y,input_command,canvas_width,canvas_height`
- Process game logic
- Output results to `output.csv` in the format: `new_player_x,new_player_y`
- Are executed through the controller when special keys are pressed

### Canvas System
The framework includes a canvas element that:
- Can render in both 2D and 3D modes
- Uses custom render functions to draw game objects
- Reads from `output.csv` to update positions of game objects
- Supports click events and can have associated handlers

## Bomberman Game Integration Strategy

### Game Module (`@module/bomber_man_v1.c`)
A new module file `@module/bomber_man_v1.c` will be created based on the current `module/xdb/game.c` but significantly expanded to handle the full Bomberman game logic:

#### Input/Output Format
- **Input (input.csv)**: `player_x,player_y,input_command,canvas_width,canvas_height`
- **Output (timestamped directory structure)**:
  - `session_YYYYMMDD_HHMMSS/` (main session directory with timestamp)
    - `timestamp_00001/` (subdirectory for first state)
      - `player.csv`: `x,y,bombs,power`
      - `enemies.csv`: Multiple lines of `id,x,y,type`
      - `bombs.csv`: Multiple lines of `id,x,y,timer`
      - `explosions.csv`: Multiple lines of `id,x,y,type`
      - `map.csv`: Multiple lines representing the grid state
      - `game_state.csv`: `score,level,lives,status,message`
      - `powerups.csv`: Multiple lines of `id,x,y,type`
    - `timestamp_00002/` (subdirectory for second state)
      - `player.csv`: `x,y,bombs,power`
      - ... (same structure as above)
    - ... (continues for each game state update)

#### Game State Management
- Maintain full game state inside the module
- Handle player movement, bomb placement, explosion propagation
- Manage enemy AI and pathfinding
- Track destructible walls, power-ups, and scoring

#### Game Logic
- Process input commands (UP, DOWN, LEFT, RIGHT, BOMB, R for restart)
- Update game world state every frame
- Handle collision detection and game events
- Implement level progression and game state (playing, game over, level complete)
- Serialize game state to multiple .stamped.csv files for UI updates

### CHTML Integration

#### New CHTML Elements
The current CHTML framework has all necessary elements for the Bomberman game:
- `<canvas>`: For game rendering
- `<text>`: For displaying game status, score, level
- `<button>`: For controls and game actions
- `<panel>`: For UI layout

#### CHTML File Structure
A new `bomberman.chtml` file will define the game UI:
- Game canvas for the main game grid
- Status panel showing score, level, lives
- Control buttons for movement and action
- Menu system for game state management (pause, restart, etc.)

#### Canvas Rendering
- The view.c canvas_render_sample function will be enhanced to render Bomberman elements
- Read from output.csv to get current game state
- Draw grid, player, enemies, bombs, explosions, walls
- Display UI elements like score and status messages

## Required CHTML Features

### Current Features (Sufficient)
- Canvas element with custom rendering
- Mouse and keyboard input handling
- Module execution via controller
- 2D rendering capabilities
- Text display for UI elements

### Missing Features (Need to Add)
1. **Timer/Animation Support**: The framework needs a way to run animations at regular intervals. Currently, the game loop would only run on input events. We need:
   - A timer callback system in the controller that calls the game module periodically
   - Game loop functionality similar to GLUT's timer function

2. **Enhanced Game State Serialization**: The current simple CSV format needs to be extended to support multiple files:
   - Multiple .stamped.csv files for different game objects
   - File management system for reading/writing multiple state files

3. **Sound Event Handling**: For audio feedback, we might need:
   - Event callbacks for sound effects (though this can be added later)

4. **More Advanced Canvas Rendering**: For better visual representation:
   - Sprite rendering capabilities
   - Color management for different game elements
   - Tile-based rendering for efficient grid drawing

### Implementation Plan

#### Phase 1: Core Module Implementation
1. Create `module/bomber_man_v1.c` with full Bomberman game logic
2. Implement map generation, player movement, bomb placement
3. Add enemy AI with pathfinding
4. Implement explosion mechanics
5. Add scoring system and level progression

#### Phase 2: CHTML Integration
1. Create `bomberman.chtml` with appropriate UI elements
2. Enhance canvas rendering to display all game elements
3. Implement input handling for game controls
4. Connect module execution to game loop

#### Phase 3: Enhancement
1. Add timer functionality for smooth animations
2. Implement JSON serialization for complex game states
3. Add visual enhancements and UI polish
4. Implement save/load functionality

## Detailed Module Interface

### Enhanced Input Format
```
player_x,player_y,input_command,canvas_width,canvas_height
```

### Enhanced Output Format (Timestamped Directory Structure)
```
session_20231003_153045/                 # Main session directory with timestamp
├── timestamp_00001/                    # First game state
│   ├── player.csv
│   │   x,y,bombs,power
│   │   40,560,1,1
│   ├── enemies.csv
│   │   id,x,y,type
│   │   0,200,480,0
│   │   1,320,200,1
│   ├── bombs.csv
│   │   id,x,y,timer
│   │   0,80,520,85
│   ├── explosions.csv
│   │   id,x,y,type
│   ├── map.csv
│   │   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
│   │   1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
│   │   ...
│   ├── game_state.csv
│   │   score,level,lives,status,message
│   │   0,1,3,Playing,Use arrow keys to move, Space to place bomb!
│   └── powerups.csv
│       id,x,y,type
├── timestamp_00002/                    # Second game state
│   ├── player.csv
│   │   x,y,bombs,power
│   │   40,520,1,1
│   ├── enemies.csv
│   │   id,x,y,type
│   │   0,200,480,0
│   │   1,320,200,1
│   ├── ...
│   └── ...
└── ...
```

## Conclusion

The current C-HTML architecture provides a solid foundation for implementing the Bomberman game, but requires a few key enhancements, primarily the addition of a game loop timer and improved game state serialization. The module system is well-suited for the game logic, and the canvas rendering system can be extended to handle the visual requirements of the game.

The architecture will follow the existing MVC pattern while extending it with game-specific functionality. The module system allows for clear separation of game logic from UI rendering, which aligns well with the original Bomberman code structure.
