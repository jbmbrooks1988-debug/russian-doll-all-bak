# Player Program

This directory contains the player program that can run games created with the RMMV-inspired editor.

## Structure

- `player_main.c` - Main orchestrator program
- `core.c` - Core engine functionality
- `managers.c` - Game state and data management
- `objects.c` - Game objects
- `scenes.c` - Scene management
- `sprites.c` - Sprite rendering
- `windows.c` - UI windows
- `plugins.c` - Plugin system
- `build.sh` - Build script
- `data/` - Directory for game data
- `build/` - Directory for build outputs

## Building

To build the player program, run:

```bash
./build.sh
```

This will compile all source files and create an executable.

## Running

To run the player with a specific project:

```bash
./player.+x <project_name>
```

The player will look for game data in the `data/<project_name>/` directory.

## How it works

When the "Play" button is pressed in the editor:
1. The current project data is copied to `player/data/<project_name>/`
2. The player program is compiled using `build.sh`
3. The player is executed with the project name as an argument
4. The player loads the game data and runs the game

## Data Format

The player expects game data in the following format:
- Map files: `map_*.txt` with columns `x,y,z,emoji_idx,fg_color_idx,bg_color_idx`
- Event files: `map_*_events.txt` with event data