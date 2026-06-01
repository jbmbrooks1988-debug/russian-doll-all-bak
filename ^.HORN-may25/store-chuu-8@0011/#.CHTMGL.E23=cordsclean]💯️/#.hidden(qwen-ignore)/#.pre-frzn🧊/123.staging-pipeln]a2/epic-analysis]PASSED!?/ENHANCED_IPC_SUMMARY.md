# Enhanced IPC System for CHTML Framework

## Overview
The CHTML framework's IPC system has been enhanced to support more flexible data exchange between modules and the UI while maintaining backward compatibility.

## Key Improvements

### 1. Enhanced Shape Format
- **3D Coordinates**: Now supports x, y, z coordinates instead of just x, y
- **Separate Dimensions**: Width, height, and depth instead of single size parameter
- **Labels**: Each shape can have an identifying label
- **Backward Compatibility**: Still supports the old "TYPE,x,y,size,r,g,b,a" format

### 2. New Message Types

#### Enhanced Shape Format
```
SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a
```
Example: `SHAPE;SQUARE;player;100.0;100.0;0.0;30.0;30.0;10.0;0.0;1.0;0.0;1.0`

#### Variable Format
```
VAR;label;type;value
```
Examples:
- `VAR;score;int;150`
- `VAR;health;int;75`
- `VAR;status;string;Game in progress`
- `VAR;player_x;float;100.00`

#### Array Format
```
ARRAY;label;type;count;value1,value2,value3...
```
Examples:
- `ARRAY;positions;float;5;10.00,20.00,30.00,40.00,50.00`
- `ARRAY;enemy_healths;int;4;80,65,90,75`

### 3. Support for Different Data Types
- **Shapes**: Enhanced with 3D coordinates, labels, and separate dimensions
- **Variables**: Support for int, float, and string types
- **Arrays**: Variable-size arrays of int or float values
- **Backward Compatibility**: Still supports the old CSV format

### 4. Implementation Details
The new system is implemented in `model.c` and works as follows:

1. **Parsing**: The `update_model()` function detects the message format by checking prefixes
2. **Enhanced Format**: Messages starting with "SHAPE:", "VAR:", or "ARRAY:" are parsed using new functions
3. **Backward Compatibility**: Messages in old format are parsed with the original CSV parser
4. **Data Storage**: New structures `ModelVariable` and `ModelArray` store additional data
5. **Memory Management**: Dynamic arrays are properly managed with malloc/free

### 5. Benefits

#### For Different Applications
- **Dynamic Webpages**: Can pass complex state variables and configuration
- **Simple 2D Games**: Can pass game state variables like score, health, etc.
- **AAA Games**: Can pass complex scene data, multiple objects, and large arrays of positions/attributes
- **Real-time Data**: Text elements automatically update with dynamic values from modules
- **3D Applications**: Full support for x, y, z coordinates and depth information

#### Scalability
- Extensible format that can easily accommodate new data types
- Variable-size arrays for dynamic content
- Labeled items for clear identification
- Efficient text-based protocol that's easy to debug

### 6. Module Example
Example module (`module/enhanced_game_ipc.c`) demonstrates:
- Sending enhanced shape data with 3D coordinates
- Sending various variable types (int, float, string)
- Sending arrays of data (positions, health values)
- Complete backward compatibility with existing functionality

### 7. Integration
- All changes are contained in the existing MVC architecture
- View and controller components automatically adapt to new data
- No changes needed to CHTML markup format
- Existing modules continue to work unchanged

This enhanced IPC system enables the CHTML framework to scale from simple applications with a few shapes to complex AAA games with diverse data requirements while maintaining simplicity and performance.