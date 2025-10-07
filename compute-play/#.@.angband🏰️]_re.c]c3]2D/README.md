# 🎮 Angband: The C & OpenGL Remake 🚀

Welcome to the grand rewrite of Angband! This project is all about remaking the classic roguelike using pure C and OpenGL, following a unique, modular approach. Let's get this adventure started! ⚔️

## 📜 Guiding Principles (The Rules of the Forge)

These are the sacred rules we'll follow on this quest:

*   **💻 Language:** Pure C (GCC)
*   **🎨 Graphics:** OpenGL / GLUT
*   **📁 Directory:** All new code lives in `c_rewrite/`
*   **🧩 Modularity:**
    *   No custom headers! 🤯
    *   We use standalone executable modules called with `system()` or `pipe()`.
    *   No linking `.c` files directly. Each module is its own hero!
*   **💾 Data:** All data shared between modules will be in human-readable `.txt` files. Easy to see, easy to debug!
*   **👣 Process:** We'll build this game one feature at a time, testing at every step.
*   **🧬 Data Structures:** We'll prefer simple arrays over structs for maximum readability.

---

## 🗺️ The Grand Plan: Our Epic Journey

Here is the map for our adventure, broken down into phases. Each phase is a milestone!

### Phase 0️⃣: ✨ The Forge - Laying the Foundation

*   **Goal:** Get a basic window up and running.
*   **Steps:**
    1.  Create the `c_rewrite/` directory.
    2.  Set up a `Makefile` for easy compilation.
    3.  Write `main.c` to initialize an OpenGL/GLUT window.
    4.  Create the main game loop.
*   **✅ Key Performance Indicators (KPIs):**
    *   A black OpenGL window appears on screen.
    *   The program compiles without errors using `make`.
    *   The window can be closed without crashing.

### Phase 1️⃣: 🌍 World-Building - Creating the Dungeon

*   **Goal:** Generate and display a game map.
*   **Steps:**
    1.  Create a `generate_map` module that outputs a dungeon layout to `map.txt`.
    2.  Create a `render_map` module that reads `map.txt` and draws it on the screen.
    3.  `main.c` will call `system("./generate_map")` once, then call `render_map` in the game loop.
*   **✅ KPIs:**
    *   A static dungeon map is visible in the window.
    *   `map.txt` exists and its contents are a clear representation of the map (e.g., using `#` for walls, `.` for floors).

### Phase 2️⃣: 🦸 The Hero's Arrival - Player Control

*   **Goal:** Get the player character on screen and moving.
*   **Steps:**
    1.  Create a `player_control` module to handle keyboard input.
    2.  This module will update the player's coordinates in `player_pos.txt`.
    3.  The main render loop will read `player_pos.txt` and draw the player character (`@`) at the correct location on the map.
    4.  Create `player_stats.txt` to hold character info (HP, etc.).
*   **✅ KPIs:**
    *   The `@` symbol appears on the map.
    *   The player can move the `@` around the dungeon using arrow keys.
    *   Movement is constrained by walls.
    *   `player_pos.txt` and `player_stats.txt` are created and updated correctly.

### Phase 3️⃣: 👹 Enter the Goblins - Monsters & NPCs

*   **Goal:** Populate the dungeon with creatures.
*   **Steps:**
    1.  Create a `monster_data.txt` file defining different monster types (e.g., `goblin,g,10,1`).
    2.  Create a `spawn_monsters` module to place monsters in the dungeon, writing their positions to `monsters_pos.txt`.
    3.  Create a `monster_ai` module that updates `monsters_pos.txt` with new positions (e.g., random movement).
    4.  The main render loop will draw all monsters from `monsters_pos.txt`.
*   **✅ KPIs:**
    *   Monster symbols (`g`, `o`, etc.) appear on the map.
    *   Monsters move around the dungeon each turn.
    *   `monsters_pos.txt` accurately reflects the positions of all monsters.

### Phase 4️⃣: ⚔️ The Clash of Steel - Combat & Items

*   **Goal:** Implement combat and item interaction.
*   **Steps:**
    1.  Update the `player_control` module to handle attack commands.
    2.  Create a `combat_resolver` module. When the player attacks, this module reads `player_stats.txt` and `monsters_pos.txt`, calculates damage, and updates the files.
    3.  Implement item pickups, storing them in `inventory.txt`.
    4.  Create `items.txt` to define all possible items in the game.
*   **✅ KPIs:**
    *   Player can attack monsters, and monsters can be defeated.
    *   Player's HP decreases when attacked by a monster.
    *   Player can pick up items from the floor.
    *   `inventory.txt` shows the player's items.

### Phase 5️⃣: 📊 The Heads-Up Display (HUD)

*   **Goal:** Display vital player information on screen.
*   **Steps:**
    1.  Create a `render_hud` module.
    2.  This module reads `player_stats.txt` and `inventory.txt`.
    3.  It displays HP, gold, inventory, and other stats on the screen, separate from the map view.
*   **✅ KPIs:**
    *   A HUD is clearly visible.
    *   The HUD updates in real-time as player stats change (e.g., taking damage).