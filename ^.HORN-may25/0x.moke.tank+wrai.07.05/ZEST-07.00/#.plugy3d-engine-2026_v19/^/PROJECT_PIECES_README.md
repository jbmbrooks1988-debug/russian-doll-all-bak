# Project Pieces System - Documentation

## Overview
The Project Pieces system enables saving/loading different game configurations from project directories. Instead of hardcoded entities, the system now loads entity definitions from configurable files located in project folders. This system also supports advanced scaling modes for specialized entities like the avocado and convenience store.

## Directory Structure
```
projects/
├── default/                    # Default project loaded at startup
│   ├── peices/               # Game pieces/entities definitions
│   │   ├── piece_map.csv     # Defines position, type, and properties of all pieces
│   │   ├── cow.txt           # Individual piece definitions
│   │   ├── pig.txt           # Individual piece definitions
│   │   └── custom_map/       # Complex multi-voxel entities
│   │       ├── entity1.txt
│   │       └── entity2.txt
│   ├── config.json           # Project configuration
│   └── metadata.json         # Project metadata (name, author, etc.)
├── project1/
│   └── ...
└── project2/
    └── ...
```

## Piece Definition Formats

### 1. Piece Map CSV (Recommended for multiple pieces)
Located at: `projects/<project_name>/peices/piece_map.csv`

Standard format with basic scaling:
```
id,type,scale_x,scale_y,scale_z,render_style,pos_x,pos_y,pos_z,color,source_file,rotation_x,rotation_y,rotation_z,movable,interactable,health
cow,emoji_entity,1.0,1.0,1.0,extruded,5,0,5,brown,data/entities/emoji/cow/cow.csv,0,0,0,True,True,100
pig,emoji_entity,1.0,1.0,1.0,extruded,10,0,1,pink,data/entities/emoji/pig/pig.csv,0,0,0,True,True,80
grass,terrain_block,1.0,1.0,1.0,six-faced,0,0,0,green,^.fin_mc_8&16_c2--]FIXD/mc_extracted_csvs_8x8/grass/grass.csv,0,0,0,False,False,0
```

Advanced format with special scaling modes:
```
id,type,scale_x,scale_y,scale_z,render_style,pos_x,pos_y,pos_z,color,source_file,rotation_x,rotation_y,rotation_z,movable,interactable,health,scale_mode,voxel_density,voxel_expansion
adam,emoji_entity,1.0,1.0,1.0,extruded,5,0,1,red,data/entities/adam/tile_8x8/1st_place_medal.csv,0,0,0,True,True,100,,,
eve,emoji_entity,1.0,1.0,1.0,extruded,6,0,3,blue,data/entities/eve/tile_8x8/2nd_place_medal.csv,0,0,0,True,True,100,,,
avocado,emoji_entity,0.25,0.25,0.25,extruded,15,0,5,green,data/entities/emoji/avocado/avocado.csv,0,0,0,True,True,90,micro,4.0,
convenience_store,emoji_entity,2.0,2.0,2.0,extruded,20,0,10,building,data/entities/emoji/convenience_store/convenience_store.csv,0,0,0,False,False,200,macro,,0.25
```

### 2. Individual Piece JSON (For complex entities)
Located at: `projects/<project_name>/peices/<piece_name>.txt`

```json
{
  "id": "avocado",
  "type": "emoji_entity", 
  "scale_x": 0.25,
  "scale_y": 0.25,
  "scale_z": 0.25,
  "render_style": "extruded",
  "pos_x": 15,
  "pos_y": 0,
  "pos_z": 5,
  "color": "green",
  "source_file": "data/entities/emoji/avocado/avocado.csv",
  "scale_mode": "micro",      // Advanced scaling: 8x8x8 fits in 2x2x2 voxel space
  "voxel_density": 4.0        // 4x smaller than default
}
```

## Supported Entity Types
- `emoji_entity`: 2D tile converted to 3D with deep extrusion (for characters, animals)
- `terrain_block`: 2D tile mapped to 6 faces of a cube (for blocks)
- `custom_model`: Loaded from 3D voxel CSV files

## Render Styles
- `extruded`: Deep extrusion from 2D tile (8 layers) - used for emoji entities
- `six-faced`: 2D tile applied to all 6 faces of a cube - used for terrain blocks
- `custom`: Loaded from 3D voxel CSV files
- `flat`: 2D tile displayed as flat plane

## Advanced Scaling Modes

### Micro Scaling (Smaller Entities)
For entities that need to be rendered smaller (fit 8x8x8 voxels into a smaller space):

- **Parameter**: `scale_mode: "micro"`
- **Effect**: Makes the entity denser with smaller voxels
- **Used by**: avocado (fits 8x8x8 voxels in 2x2x2 voxel space)
- **Control parameter**: `voxel_density` (higher = smaller voxels packed closer)

**Example to make avocado even smaller**:
- Increase `voxel_density` from 4.0 to 8.0 (or higher)
- Adjust `scale_x`, `scale_y`, `scale_z` to 0.125 (or smaller)

### Macro Scaling (Bigger Entities)
For entities that need to be rendered larger (spread 8x8x8 voxels across more space):

- **Parameter**: `scale_mode: "macro"`
- **Effect**: Expands each voxel to cover more area
- **Used by**: convenience_store (8x8x8 voxels spread across 2x2x2 grid space)
- **Control parameter**: `voxel_expansion` (lower = bigger spread)

**Example to make convenience store even bigger**:
- Decrease `voxel_expansion` from 0.25 to 0.125 (or lower)
- Increase `scale_x`, `scale_y`, `scale_z` to 3.0 (or bigger)

### Manual Adjustment Guide
1. **To make avocado BIGGER**: 
   - Decrease `voxel_density` value (e.g., from 4.0 to 2.0)
   - Increase `scale_x`, `scale_y`, `scale_z` values (e.g., from 0.25 to 0.5)

2. **To make convenience store SMALLER**:
   - Increase `voxel_expansion` value (e.g., from 0.25 to 0.5)
   - Decrease `scale_x`, `scale_y`, `scale_z` values (e.g., from 2.0 to 1.5)

3. **General scaling**:
   - Use `scale_x`, `scale_y`, `scale_z` for uniform or axis-specific scaling
   - Use `scale_mode` with `voxel_density` or `voxel_expansion` for advanced effects

## Usage

### Default Setup (Restored)
- The system now restores the original default behavior where entities (adam, eve, cow, pig, etc.) and terrain render automatically
- The Entity Manager loads default entities at initialization (cobblestone, dirt, stone, grass, wood)
- Adam and Eve are loaded from the default project configuration (not hardcoded)
- Advanced entities (avocado, convenience store) are loaded with special scaling

### Creating a New Project
1. Use the Project menu: "New Project..."
2. Or programmatically via ProjectManagerPlugin

### Saving Current State
1. Use the Project menu: "Save Project"
2. Current grid state is saved to `piece_map.csv`

### Loading a Project
1. Use the Project menu: "Load Project..."
2. Or the system will automatically load the "default" project at startup
3. The "default" project includes all entities (adam, eve, cow, pig, avocado, convenience store) and can be extended with more entities and terrain

### Project Fallback System
- If no specific pieces are defined in a project, the system creates default demo pieces (cow, pig, basic terrain)
- This ensures visual content is always present in the scene
- Original entity loading behavior is preserved for backward compatibility

## Implementation Notes
- The ProjectManagerPlugin is now part of the core plugins
- The system consolidates entity management from multiple plugins into a unified system
- Backwards compatibility is maintained with existing entity and terrain systems
- The default project is automatically loaded at startup if available
- Advanced scaling modes are handled by the EntityRendererPlugin