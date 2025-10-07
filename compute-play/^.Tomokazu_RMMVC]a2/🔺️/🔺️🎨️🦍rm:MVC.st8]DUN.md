# Emoji Paint: Guerrilla Games' Tomokazu Sweet App Dev Pipeline Template 🚀🎨🍒

Hey dev team! 🌟 Welcome to **Emoji Paint**, our fun little OpenGL-based painting app where you can doodle with emojis like 🍋, 💎, and 🔔! It's built with GLUT for graphics, FreeType for emoji rendering, and follows the **Model-View-Controller (MVC)** pattern. The big vision? Make this a reusable template for Guerrilla Games' *Tomokazu Sweet* pipeline: tweak `model_gl.c` for new games, while keeping `main.c`, `controller_gl.c`, and `view_gl.c` as static, unchanging foundations. Let's analyze where we are, how close we're vibing to the goal, and plot our emoji-fueled path forward! 🛤️💥

## Current State Analysis 🔍🍏

Right now, the app is a solid emoji-painting playground:
- **Core Features**:
  - 2D/3D views (toggle with buttons or keys) 👀📐
  - Tools: Paint 🖌️, Fill 🪣, Rectangle 📏
  - Layers (up to 10 Z-levels) 🥞
  - Emoji palette (64 fruity/fun options like 🍓 and 🥑)
  - Color palette (8 basics: Red ❤️, Green 💚, etc.)
  - Tab bank for quick emoji+color combos 📑
  - Save/load to CSV 💾
  - Camera controls in 3D (WASD + QE) 📹
  - ASCII grid in terminal for debugging 🖥️
- **MVC Breakdown**:
  - **`model_gl.c`** (The Brain 🧠): Holds data like `Tile` struct, `canvas` array, globals (e.g., `selected_emoji`), getters/setters for everything, init logic, game functions (flood_fill 🌊, draw_rectangle 🟥, save/load 📁), and even `print_ascii_grid` for terminal output.
  - **`main.c`** (The Launcher 🚀): Sets up GLUT window, computes canvas size, hooks up callbacks. Minimal game-specific stuff—mostly boilerplate.
  - **`controller_gl.c`** (The Input Wizard 🕹️): Handles mouse clicks 🖱️, keyboard presses ⌨️, special keys (arrows ➡️), and idle checks. Calls model getters/setters to update state (e.g., `set_selected_tool(1)` for fill).
  - **`view_gl.c`** (The Artist 🎨): Renders everything—sidebar UI, canvas in 2D/3D, emojis with FreeType, borders, grids. Uses model getters to fetch data (e.g., `get_canvas_tile()`).
- **Tech Stack Vibes**:
  - OpenGL/GLUT for rendering 🖼️
  - FreeType for colorful emojis (loads NotoColorEmoji.ttf) 🌈
  - Terminal integration for ASCII debug and input (non-blocking with select()) 🛠️
  - 3D mode uses gluPerspective and rotations for that immersive feel 🔄

The code runs smooth (assuming you have the fonts and libs installed—check `/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf`! 📂). But is it pipeline-ready? Let's assess! 📊

## Assessment: How Close Are We to the Goal? 📏🥝

Goal: `model_gl.c` = Game-Specific Magic ✨ (change this for new apps like a puzzle game or RPG). `main.c`, `controller_gl.c`, `view_gl.c` = Static Reusables 🗿 (no tweaks needed across projects).

- **Closeness Score: 7/10 🍐** (Solid foundation, but some leaks in separation! 🚧)
  - **Wins** 🎉:
    - Model exposes clean getters/setters (e.g., `get_selected_emoji()`, `set_camera_yaw()`)—view and controller use these, not direct globals. This keeps things modular! 🔌
    - Game logic (flood_fill, rectangle drawing) is isolated in model. Perfect for swapping in new mechanics like "match-3" or "pathfinding" in future games. 🧩
    - Init is split: `init_model()` in model, full `init()` calls it + FreeType. View uses model data for rendering without hard dependencies.
    - Controller handles inputs generically (e.g., maps keys to model calls)—could reuse for different games with minimal changes.
    - Main is super static: Just GLUT setup and callback hooks. No game logic here! 👍
  - **Gaps** 😅:
    - `print_ascii_grid()` is in model but it's pure view/debug (terminal output with ANSI colors 🎨). Model should be UI-agnostic— no printing!
    - Some globals in model (e.g., `emojis[]`, `colors[]`) are accessed via getters, but view/controller import model headers, creating tight coupling. Could be more abstracted.
    - 3D camera and Z-level logic is in model (getters/setters + movement funcs like `move_camera_forward()`). This is game-specific (Emoji Paint has 3D layers), but if a future game is 2D-only, it's bloat. 🏋️
    - View has hard-coded UI elements (e.g., sidebar positions, emoji grid layout) tied to this app's features. For reuse, it needs to query model for what to draw (e.g., "give me palettes").
    - Controller assumes specific tools/keys (e.g., 'f' for fill, 'z'/'x' for Z-level). This is Emoji Paint-specific—needs generalization for other games.
    - FreeType init is in model (`initFreeType()` called from `init()`), but it's view-related (rendering). Move it! 📦
    - Terminal input in controller (e.g., `check_terminal_input()`) mixes with GLUT—fine for this hybrid app, but for pure GLUT games, it might not fit.

Overall, we're close: Model is mostly logic/data, but some view bleed-in. Static files are reusable with tweaks, but not fully plug-and-play yet. Time to level up! ⬆️

## Suggested Changes to Achieve the Goal 🛠️🥑

To hit 10/10 and make this a killer template:
1. **Purge View/Debug from Model** ❌🖥️:
   - Move `print_ascii_grid()` to a new `debug.c` or into `view_gl.c` (as an optional terminal render). Model shouldn't know about terminals or ANSI!
   - Shift `initFreeType()` to `view_gl.c`—view owns rendering.

2. **Abstract Model Interfaces** 🔑📚:
   - Add more high-level model funcs if needed (e.g., `get_palette_size()`, `get_tool_names()` for view to query dynamically).
   - Ensure no direct global access: All via getters/setters. (Already good, but double-check imports.)
   - For 3D: Make camera/Z-level optional. Add a model flag like `is_3d_enabled()`—view checks this to toggle modes.

3. **Generalize Controller** 🕹️🔄:
   - Use configurable keymaps (e.g., model provides a `handle_key(char key)` func that controller calls).
   - Extract app-specific logic (e.g., tool switching, camera moves) into model callbacks. Controller just forwards inputs.

4. **Make View More Dynamic** 🎨🧩:
   - View should render based on model queries (e.g., loop over `get_num_emojis()` instead of hard-coding 64).
   - Add model funcs like `get_ui_elements()` for sidebar tabs/buttons—lets future games customize UI without touching view.

5. **Enhance Reusability** 🔄🍓:
   - Add preprocessor flags (e.g., `#ifdef DEBUG_TERMINAL`) for optional features like ASCII grid.
   - Document model as the "mod point": "Want a new game? Swap flood_fill with your AI logic! 🤖"
   - Test: Build a mini variant (e.g., emoji puzzle) by only changing model.

These changes keep main/controller/view untouched for new projects—pure emoji magic! ✨

## Future Plan: Roadmap to Pipeline Perfection 🗺️💎

Short-term (Next Sprint 🏃‍♂️):
- Implement suggestions 1-2: Clean model, move debug/print. Target: 1 week. 🍋
- Test with a mock "new game" (e.g., add a simple emoji-matching mechanic in model only). 🧪

Mid-term (Pipeline Beta 🚧):
- Generalize controller/view per 3-4: Add dynamic queries. 2 weeks.
- Create 2-3 example games (e.g., Emoji Tic-Tac-Toe ❌⭕, 3D Emoji Tower 🏗️) using the template. Share internally! 📤

Long-term (Tomokazu Sweet Launch 🌟):
- Integrate with Guerrilla's tools (e.g., auto-gen getters/setters script? 🤖).
- Open-source? (With emoji docs! 📖)
- Expand: Add networking for collab painting 👥, more emojis 🍄.

Let's crush this, team—emoji power! 🎉 If questions, ping me. 💬

*Last updated: September 06, 2025* 📅
📌️📌️📌️📌️📌️📌️📌️📌️📌️📌️📌️📌️📌️📌️📌️📌️📌️
1.Move print_ascii_grid() into view_gl.c2.Shift initFreeType() to view_gl.c—view owns rendering.3.Add model funcs like get_ui_elements() for sidebar tabs/buttons—lets future games customize UI without touching view.
+EARLIER (pre-3d , forgotten)

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

-------------------------------------] w8 on this, till we are making games internally as well. and refold sword .
6. **Test with a New Game** 🎲:
   - Create a simple game (e.g., a basic platformer) by modifying only `model_gl.c`.
   - Confirm that the template supports it without touching other files.
===========================================================
