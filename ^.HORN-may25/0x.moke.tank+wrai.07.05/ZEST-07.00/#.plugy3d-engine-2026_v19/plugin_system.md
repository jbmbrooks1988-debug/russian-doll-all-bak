# Fresh Engine 2026 - Plugin System

## Plugin Architecture

This directory contains the plugin system for the fresh engine. Plugins follow a standard interface to integrate seamlessly with the main application.

## Directory Structure

```
plugins/
├── core/              # Core functionality plugins
│   ├── voxel_grid/    # Voxel grid rendering and interaction
│   ├── camera_controller/ # Camera controls and views  
│   ├── entity_manager.py  # Entity loading and management
│   ├── project_manager.py # Project loading and management (includes piece system)
│   └── selector/      # Selection tools
├── rendering/         # Rendering plugins
│   └── entity_renderer.py # Optimized entity rendering with VBO support
├── entities/          # Entity-specific plugins
│   ├── emoji_entity_manager.py # 3D emoji entities from 2D tiles
│   └── npc/           # NPC entities
├── terrain/           # Terrain generation
│   └── terrain_generator.py # Minecraft-style terrain from CSV data
└── ui/               # User interface plugins
    ├── minimap.py     # Minimap display
    ├── new_hud.py     # HUD elements
    ├── menu/          # Menu systems
    └── time/          # Time-related UI elements
```

## Plugin Interface

All plugins must implement the following interface:

```python
class BasePlugin:
    def __init__(self, main_app):
        self.main_app = main_app
        self.enabled = True
    
    def initialize(self):
        """Called when the plugin is loaded"""
        pass
    
    def activate(self):
        """Called when the plugin is activated"""
        self.enabled = True
    
    def deactivate(self):
        """Called when the plugin is deactivated""" 
        self.enabled = False
    
    def get_widget(self):
        """Return the widget for UI integration if applicable"""
        return None
```

## Core Plugins

### voxel_grid
Handles the 3D voxel grid rendering, coordinate system, and basic grid operations. Acts as the central widget that other plugins modify and enhance.

### camera_controller
Manages camera positioning, rotation, and view controls.

### selector
Provides tools for selecting and manipulating voxels/entities.

### entity_manager
Handles loading and managing 3D entities from various sources including Minecraft textures and emoji tile data.

### project_manager
Manages loading and saving of game projects from the projects/ directory. Implements the piece system for loading entities from project files.

## Project and Piece System

### Overview
The project system allows for loading and saving complete game states from project directories within the `projects/` folder. Each project can contain multiple "pieces" which represent game entities, terrain blocks, and other interactive elements.

### Project Structure
```
projects/
└── <project_name>/
    ├── metadata.json          # Project metadata and configuration
    └── peices/               # Contains individual pieces
        ├── piece_map.csv     # CSV-based piece definitions (highest priority)
        ├── <piece_name>.txt  # Individual JSON-formatted piece definitions
        └── ...
```

### Piece System Integration
Both piece map files and individual piece files are loaded together:
1. **piece_map.csv files**: Multiple CSV files (patterns like `*.piece_map.csv`) containing structured piece assemblies (e.g., buildings, complex structures)
2. **Individual .txt files**: Standalone JSON-formatted pieces that can be placed independently

Each piece definition includes:
- Basic properties: `id`, `type`, `pos_x`, `pos_y`, `pos_z`, `color`
- Scaling: `scale_x`, `scale_y`, `scale_z`
- Rendering: `render_style` (extruded or six-faced), `source_file`
- Advanced features: `scale_mode` (micro/macro), `voxel_density`, `voxel_expansion`
- Behavior: `movable`, `interactable`, `health`

### Supported Entity Types
- **emoji_entity**: 3D entities loaded from 2D tile CSV files with depth extrusion
- **terrain**: Terrain blocks loaded from Minecraft texture CSVs
- **npc**: Non-player character entities

### Piece Map Assemblies
Piece map files represent building assemblies or complex structures that can contain multiple pieces in fixed spatial relationships:
- house.piece_map.csv: Contains all pieces that form a house (walls, roof, doors, windows)
- castle.piece_map.csv: Contains pieces forming a castle structure
- vehicle.piece_map.csv: Contains pieces forming vehicles or machines

### Loading Process
1. Project Manager scans for ALL `*.piece_map.csv` files in the project's peices directory
2. Loads each piece_map.csv file as a structured assembly
3. Loads all individual `.txt` files as standalone pieces
4. Combines both sets of pieces into the final project
5. If no files exist, creates default demo pieces
6. Applies all pieces to the voxel grid, combining entities and terrain blocks