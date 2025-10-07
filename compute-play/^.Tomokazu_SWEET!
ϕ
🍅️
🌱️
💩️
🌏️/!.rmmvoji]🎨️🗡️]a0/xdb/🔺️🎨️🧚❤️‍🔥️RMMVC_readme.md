🔺️🎨️🧚❤️‍🔥️
# 🦍 RMMVC.md: MVC Template Analysis for Tomokazu Sweet App Dev Pipeline 🍬

## 🎮 Overview: Current MVC Structure for Emoji Paint 🖌️
The provided code (`main.c`, `model_gl.c`, `controller_gl.c`, `view_gl.c`) implements an **Emoji Paint** application using OpenGL (GLUT) and FreeType for rendering emojis. It follows a **Model-View-Controller (MVC)** pattern, designed as a template for Guerrilla Games' *Tomokazu Sweet* app dev pipeline. The goal is to make `model_gl.c` the primary file for game-specific modifications, keeping `main.c`, `controller_gl.c`, and `view_gl.c` static for reuse across different games. Below, we analyze the current state, assess how close the code is to this goal, and suggest changes to achieve it, all with an emoji-heavy vibe for the dev team! 🚀

---

## 🧠 How the Code Currently Works: MVC Breakdown

### 🗃️ Model (`model_gl.c`) 📊
The model manages the **game state** and **logic**:
- **Data Structures**:
  - `Tile` struct: Stores `emoji_idx`, `fg_color`, `bg_color` for each canvas tile.
  - `canvas[2][MAX_CANVAS_DIM][MAX_CANVAS_DIM]`: A 2-layer grid (max 128x128) for painting emojis with foreground/background colors.
  - `tab_bank[10]`: Stores up to 10 saved emoji+color combos.
  - Arrays for `emojis[64]` and `colors[8][3]` (RGB values) and `color_names[8]`.
- **State Variables**:
  - Canvas size (`canvas_rows`, `canvas_cols`), `tile_size`, `sidebar_width`, `file_tab_height`, `window_width`, `window_height`.
  - `selected_emoji`, `selected_fg_color`, `selected_bg_color`, `selected_tool` (0: Paint, 1: Fill, 2: Rectangle).
  - `selector_row`, `selector_col` for cursor position; `start_row`, `start_col` for rectangle tool.
  - `show_all_layers` toggles layer visibility in the terminal.
- **Functions**:
  - **Getters**: Retrieve state (e.g., `get_canvas_rows()`, `get_selected_emoji()`).
  - **Setters**: Update state (e.g., `set_canvas_tile()`, `set_status_message()`).
  - **Logic**: `flood_fill()` for filling areas, `draw_rectangle()` for drawing rectangles, `save_canvas()`/`load_canvas()` for CSV-based persistence, `print_ascii_grid()` for terminal output.
- **Initialization**: `init_model()` clears the canvas with default values; `init()` sets up FreeType.

### 🎨 View (`view_gl.c`) 🖼️
The view handles **rendering** using OpenGL and FreeType:
- **Rendering**:
  - `display()`: Clears the screen, sets up an orthographic projection, and draws:
    - Canvas grid with background tiles.
    - Emojis on the canvas using `render_emoji()`, with foreground/background colors.
    - Tile selector (yellow border) for the cursor or rectangle region.
    - Sidebar with file tabs ("Save", "Load", etc.), emoji grid (8x8), color palette, tab bank, and tool buttons.
    - Status message at the bottom.
  - `render_emoji()`: Loads emoji glyphs with FreeType, creates OpenGL textures, and draws them with scaling.
  - `render_text()`: Draws text using GLUT bitmap fonts.
  - `draw_rect()`/`draw_border()`: Utility functions for drawing rectangles and borders.
- **FreeType**: `initFreeType()` loads the Noto Color Emoji font and sets pixel size.
- **Dependencies**: Relies on model getters to access state (e.g., `get_canvas_rows()`, `get_emoji()`).

### 🎮 Controller (`controller_gl.c`) 🕹️
The controller handles **user input** and updates the model:
- **Input Handling**:
  - `mouse()`: Processes left-clicks on the canvas (paint/fill/rectangle), file tabs, emoji grid, color palette, tab bank, or tools.
  - `keyboard()`: Handles keys like `space`, `Enter`, `f` (fill), `r` (rectangle), `1` (cycle emoji), `c` (cycle color), `2` (toggle layers), `s` (save), `l` (load), `t` (create tab), `q` (quit).
  - `special()`: Moves the selector with arrow keys.
  - `idle()`: Checks terminal input for the same keys as `keyboard()`.
  - `reshape()`: Updates window size and viewport.
- **Logic**:
  - Updates model state via setters (e.g., `set_canvas_tile()`, `set_selected_tool()`).
  - Calls model functions like `flood_fill()`, `draw_rectangle()`, `save_canvas()`, `load_canvas()`, and `print_ascii_grid()`.
  - Triggers redisplay with `glutPostRedisplay()`.

### 🚀 Main (`main.c`) 🌟
The entry point sets up the application:
- Initializes terminal settings (non-canonical, no echo) for input.
- Queries screen size via Xlib to set near-fullscreen window dimensions.
- Computes canvas size based on `window_width`, `sidebar_width`, `tile_size`, and `file_tab_height`.
- Initializes GLUT, sets up callbacks (`display`, `reshape`, `mouse`, `keyboard`, `special`, `idle`), and starts the main loop.
- Calls `init()` to set up the model and FreeType.

---

## 🥗 How Close Are We to the Goal? 🍎
The goal is to make `model_gl.c` the **only** file modified for new games, keeping `main.c`, `controller_gl.c`, and `view_gl.c` static. Here's the assessment:

### ✅ What's Working Well
- **Model Isolation** 🏆:
  - `model_gl.c` encapsulates all game state and logic (canvas, tiles, tools, persistence).
  - Getters and setters provide a clear interface for the view and controller to access/modify state without directly manipulating data structures.
- **View Generality** 🖌️:
  - `view_gl.c` uses model getters to render the canvas, sidebar, and UI elements, making it data-driven.
  - Rendering functions (`render_emoji`, `draw_rect`) are reusable for grid-based games with emojis or colors.
- **Controller Flexibility** 🎮:
  - `controller_gl.c` handles input generically, mapping user actions to model setters and logic functions.
  - Input handling (mouse, keyboard, terminal) is abstract enough to support various game mechanics via model updates.
- **Main Simplicity** 🚀:
  - `main.c` is minimal, handling only initialization and GLUT setup, which is ideal for a static entry point.

### ⚠️ Gaps and Limitations
1. **Model Specificity** 😓:
   - The model is tailored to **Emoji Paint**:
     - Fixed `Tile` struct with `emoji_idx`, `fg_color`, `bg_color`.
     - Hardcoded `emojis[64]` and `colors[8][3]`.
     - Logic functions (`flood_fill`, `draw_rectangle`) are specific to painting mechanics.
     - Persistence (`save_canvas`, `load_canvas`) assumes a specific CSV format.
   - New games (e.g., a platformer or puzzle game) may need different data structures, state variables, or logic, requiring significant changes to `model_gl.c` beyond just swapping logic.
2. **View Assumptions** 🖼️:
   - The view assumes a **grid-based canvas** with emojis, colors, and a specific sidebar layout (file tabs, emoji grid, tools).
   - Rendering is tied to `tile_size`, `canvas_rows`, `canvas_cols`, and emoji fonts, limiting flexibility for non-grid games or different UI layouts.
3. **Controller Assumptions** 🕹️:
   - Input handling is designed for painting tools (paint, fill, rectangle) and specific UI interactions (emoji/color selection, file tabs).
   - Key bindings (`f`, `r`, `1`, `c`, etc.) and mouse click regions are hardcoded for Emoji Paint's UI.
   - Non-grid or non-painting games may need different input mappings or UI interactions.
4. **Hardcoded Constants** 🔢:
   - Constants like `MAX_CANVAS_DIM` (128), `max_layers` (2), `num_emojis` (64), `num_colors` (8), `max_tabs` (10), and `tile_size` (32) are defined in `model_gl.c` and used across files, reducing flexibility.
   - Sidebar and file tab layouts are hardcoded in `view_gl.c` and `controller_gl.c`, making UI changes require edits to multiple files.
5. **Persistence Format** 📁:
   - The CSV-based save/load system is specific to the `Tile` struct and canvas layout, limiting reuse for games with different state representations.
6. **Font Dependency** 🖍️:
   - The view relies on the Noto Color Emoji font, which may not suit games without emoji-based graphics.

### 📏 Closeness to Goal
We're **partially there** (~60% of the way) 🎯:
- The MVC separation is solid, with clear interfaces via getters/setters.
- `main.c` is already static and reusable.
- `controller_gl.c` and `view_gl.c` are generic in their input handling and rendering approach but are tightly coupled to Emoji Paint's grid-based, emoji-driven design.
- `model_gl.c` is the primary place for game logic, but its data structures and functions are too specific to support diverse games without significant rewrites.

To achieve the goal, we need to **generalize** the model, view, and controller to support a wider range of games while keeping `model_gl.c` the sole modification point.

---

## 🛠️ Recommended Changes for a Fully Reusable MVC Template

### 1. Generalize the Model (`model_gl.c`) 🌟
Make the model abstract enough to support various game types (e.g., grid-based, sprite-based, or physics-based games).

- **Abstract Data Structures** 🗃️:
  - Replace the `Tile` struct with a generic `GameObject` struct or union:
    ```c
    typedef struct {
        int type; // e.g., 0=tile, 1=sprite, 2=entity
        union {
            struct { int emoji_idx; int fg_color; int bg_color; } tile;
            struct { float x, y; int sprite_id; } sprite;
            // Add other game object types as needed
        } data;
    } GameObject;
    ```
    - Allow games to define custom object properties in `model_gl.c`.
  - Use a dynamic array or list for game objects instead of a fixed `canvas[2][MAX_CANVAS_DIM][MAX_CANVAS_DIM]`:
    ```c
    GameObject* game_objects;
    int object_count;
    ```
  - Generalize `tab_bank` to a `GameStateBank` for storing arbitrary state snapshots.

- **Flexible State Variables** 🔧:
  - Replace specific variables (`canvas_rows`, `selected_emoji`, etc.) with a key-value store or struct:
    ```c
    typedef struct {
        char* key;
        union { int i; float f; bool b; void* ptr; } value;
    } GameState;
    GameState* state; // Dynamic array
    int state_count;
    ```
    - Provide generic getters/setters:
      ```c
      void set_state(const char* key, int value); // Or float, bool, etc.
      int get_state_int(const char* key);
      ```
    - Example: Replace `set_canvas_rows(value)` with `set_state("canvas_rows", value)`.

- **Generic Logic Functions** 🧩:
  - Replace `flood_fill` and `draw_rectangle` with a generic `update_game_state` function:
    ```c
    void update_game_state(int action_id, void* params);
    ```
    - Games define `action_id` (e.g., 0=paint, 1=move_sprite) and pass parameters via `params`.
  - Keep `save_canvas`/`load_canvas` generic by serializing `GameObject` and `GameState` to a flexible format (e.g., JSON instead of CSV):
    ```c
    void save_game_state(const char* filename);
    void load_game_state(const char* filename);
    ```

- **Initialization**:
  - Make `init_model()` a game-specific function in `model_gl.c` to set up custom objects and state:
    ```c
    void init_model(void); // Define in model_gl.c for each game
    ```

### 2. Make the View Generic (`view_gl.c`) 🎨
Ensure the view can render any game state without modification.

- **Abstract Rendering**:
  - Replace grid-specific rendering with a generic `render_object` function:
    ```c
    void render_object(GameObject* obj, float x, float y);
    ```
    - For Emoji Paint, render emojis; for other games, render sprites or shapes based on `obj->type`.
  - Use a callback for game-specific rendering:
    ```c
    void (*render_game_object)(GameObject* obj, float x, float y); // Set in model_gl.c
    ```
  - Generalize UI rendering:
    - Replace hardcoded sidebar/file tab layout with a configurable UI element list:
      ```c
      typedef struct {
          char* label;
          float x, y, w, h;
          int action_id; // Triggered when clicked
      } UIElement;
      UIElement* ui_elements;
      int ui_element_count;
      ```
    - Render UI elements in `display()` using model-provided data.

- **Flexible Coordinates**:
  - Replace `tile_size`, `canvas_rows`, etc., with model-driven coordinates:
    ```c
    float get_object_x(GameObject* obj);
    float get_object_y(GameObject* obj);
    ```

- **Font Handling**:
  - Make font loading optional (e.g., check a model flag `use_emoji_font`).
  - Support multiple fonts or sprite sheets for non-emoji games.

### 3. Generalize the Controller (`controller_gl.c`) 🕹️
Make input handling reusable for any game.

- **Abstract Input Mapping**:
  - Replace hardcoded key bindings (`f`, `r`, `1`, etc.) with a configurable input map:
    ```c
    typedef struct {
        char key; // Or mouse button, special key
        int action_id; // Maps to model action
    } InputBinding;
    InputBinding* input_bindings;
    int binding_count;
    ```
    - Example: Map `f` to `action_id=1` (fill) for Emoji Paint, or `jump` for a platformer.
  - Update `mouse`, `keyboard`, `special`, and `idle` to call `update_game_state(action_id, params)` based on the input map.

- **UI Interaction**:
  - Replace hardcoded click regions with checks against `UIElement` list:
    ```c
    void handle_ui_click(float x, float y); // Checks ui_elements and calls update_game_state
    ```

- **Generic Input Loop**:
  - Consolidate `keyboard` and `idle` to avoid redundant code:
    ```c
    void handle_input(char key, int x, int y, bool is_special);
    ```

### 4. Keep `main.c` Static 🚀
- **Current State**: Already minimal and reusable, setting up GLUT and terminal.
- **Enhancement**:
  - Add a model callback to initialize game-specific settings:
    ```c
    void (*init_game_settings)(void); // Defined in model_gl.c
    ```
  - Call `init_game_settings()` before GLUT initialization to allow games to set window size, title, etc.

### 5. Centralize Constants and Configuration ⚙️
- Move all constants (`MAX_CANVAS_DIM`, `max_layers`, `num_emojis`, etc.) to `model_gl.c` or a separate `config.h` included only by `model_gl.c`.
- Define UI layout, input bindings, and rendering callbacks in `model_gl.c`:
  ```c
  void configure_game(void); // Sets ui_elements, input_bindings, render_game_object, etc.
  ```

### 6. Example Game Implementation in `model_gl.c` 🕹️
For Emoji Paint, `model_gl.c` would define:
- `GameObject` as `Tile` with emoji/color properties.
- State keys like `"canvas_rows"`, `"selected_emoji"`.
- Actions like `update_game_state(0, &tile_data)` for painting.
- UI elements for file tabs, emoji grid, etc.
- Input bindings mapping `f` to fill, etc.
- Rendering callback to call `render_emoji`.

For a platformer, `model_gl.c` would define:
- `GameObject` as sprites with position, velocity, sprite_id.
- State keys like `"player_x"`, `"score"`.
- Actions like `update_game_state(1, &jump_data)` for jumping.
- UI elements for score display, pause menu.
- Input bindings mapping `space` to jump.
- Rendering callback to draw sprites.

---

## 🗺️ Implementation Plan for Dev Team

1. **Refactor Model** 🛠️:
   - Replace `Tile` with `GameObject`.
   - Use dynamic arrays for objects and state.
   - Implement generic `update_game_state`, `save_game_state`, `load_game_state`.
   - Move constants to `model_gl.c`.

2. **Refactor View** 🎨:
   - Add `render_game_object` callback.
   - Implement `UIElement` list for dynamic UI rendering.
   - Generalize coordinate calculations using model getters.

3. **Refactor Controller** 🕹️:
   - Implement `InputBinding` list for configurable inputs.
   - Update input handlers to use `update_game_state`.
   - Add `handle_ui_click` for UI interactions.

4. **Update Main** 🚀:
   - Add `init_game_settings` callback.
   - Ensure terminal and GLUT setup remain static.

5. **Test with Emoji Paint** ✅:
   - Reimplement Emoji Paint in the new template by defining game-specific logic in `model_gl.c`.
   - Verify that `main.c`, `controller_gl.c`, and `view_gl.c` are unchanged.

6. **Test with a New Game** 🎲:
   - Create a simple game (e.g., a basic platformer) by modifying only `model_gl.c`.
   - Confirm that the template supports it without touching other files.

---

## 🎉 Benefits of the Refactored Template
- **Single File Modification** 📝: All game-specific logic, data, UI, and input mappings live in `model_gl.c`.
- **Reusability** ♻️: `main.c`, `controller_gl.c`, and `view_gl.c` are static and support diverse games.
- **Flexibility** 🧩: Supports grid-based, sprite-based, or other game types via abstract `GameObject` and callbacks.
- **Scalability** 🚀: Dynamic arrays and configurable UI/input allow complex games without hardcoding.

---

## 🐒 Guerrilla Games Dev Team: Let's Get to Work! 💪
This template is a solid start but needs generalization to hit the *Tomokazu Sweet* pipeline goal. By abstracting the model, view, and controller as outlined, we'll create a **plug-and-play** MVC framework where new games only require tweaking `model_gl.c`. Let's make it sweet! 🍬

**Next Steps**:
- Assign tasks for refactoring model, view, and controller.
- Prototype the new template with Emoji Paint and a test game.
- Document the `model_gl.c` API for game developers.

Happy coding, team! 🦍🎮
