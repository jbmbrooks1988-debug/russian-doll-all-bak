# 🗺️ RPGAtlas Code Walkthrough

Welcome to the **RPGAtlas** inner workings! This document provides a full tour of the engine's architecture, from procedural pixels to the game loop.

---

## 📂 Project Structure at a Glance

*   `index.html`: The Editor shell 🛠️.
*   `play.html`: The Player shell ▶️.
*   `js/assets.js`: The "Art Department" — generates all tiles and sprites 🎨.
*   `js/sfx.js`: The "Sound Studio" — generates all sounds and music 🎵.
*   `js/data.js`: The "Brain" — defines the schema and sample game 🧠.
*   `js/engine.js`: The "Heart" — the game runtime and renderer ⚙️.
*   `js/editor.js`: The "Workbench" — all tools for building your world 🛠️.
*   `js/plugins.js`: The "Expansion Pack" — built-in extensibility 🔌.

---

## 🎨 Procedural Assets (`js/assets.js`)

Unlike traditional engines that load PNGs, RPGAtlas *draws* its own graphics using the HTML5 Canvas API.

*   **Tiles**: Functions like `drawHuman` and `defTile` use math and RNG seeds to create consistent grass, water, and brick patterns.
*   **Characters**: Uses a 16x16 logical grid (scaled to 48x48) to draw "Humans" with customizable skin, hair, and clothing.
*   **Enemies**: Distinct silhouettes (slime, bat, golem) are procedurally generated and tinted.
*   **Caching**: To keep things fast, every tile and sprite frame is rendered once and stored in a cache (`tileCache`, `charCache`).

## 🎵 Sound & Generative Music (`js/sfx.js`)

Uses the **Web Audio API** to synthesize chiptune sounds on the fly.

*   **SFX**: `tone()`, `noise()`, and `arp()` create everything from sword hits to magical heals without any audio files.
*   **Generative Music**: Themes like `town` and `battle` define a tempo, root note, and scale. A "Mulberry" RNG creates a unique but consistent melody walk every time you play.

## 📊 Data & Schema (`js/data.js`)

This file defines what an RPGAtlas "Project" actually is.

*   **Migration**: `migrateProject` ensures that as the engine evolves, your old save files and projects still work.
*   **Sample Game**: Contains the code to build *Atlas Quest*, the default adventure you see when you first open the editor.

## ⚙️ The Game Engine (`js/engine.js`)

The runtime that makes the game "go".

*   **Input**: Manages the `UIStack` to handle menu navigation and player movement.
*   **Interpreter**: The `Interp` class reads the command lists from events (Show Text, Battle, etc.) and executes them sequentially.
*   **Battle System**: A turn-based combat engine with side-view animations, status effects, and particle pools for performance.
*   **Scene Management**: Switches between `boot`, `title`, `map`, and `battle`.

## 🛠️ The Editor (`js/editor.js`)

A massive file that handles the visual building experience.

*   **DOM Builder**: Uses a tiny helper function `h()` to create complex UI modals and forms without a heavy framework like React.
*   **Tools**: Implements the Pen, Fill, Shadow, and selection logic.
*   **Undo/Redo**: Uses a snapshot system to store the state of the map, making mistakes easy to fix.

## 🔌 Plugin System (`js/plugins.js`)

Allows developers to inject custom JavaScript into the engine.

*   **Bridge**: The `atlas` object provides a safe API for plugins to hook into map loading, rendering, and custom commands.
*   **Built-ins**: Ships with Core helpers, Text Codes (BBCode), Transitions, and Weather effects.

---

# 🚀 Future Facing Features

### ⚖️ Copyright & Originality (The "Clean Room" Approach)
Before we dive into the features, let's address the common concern: **Is it legal to mimic the original RPG Maker's architecture?**

**Short Answer: Yes.** Here is why the prototype refactor is legally safe:

1.  **Expression vs. Idea**: Copyright protects the *literal expression* of code (the specific lines of text), not the *idea* or *architecture*. Using JavaScript's built-in `prototype` system is a standard language feature, not a proprietary invention.
2.  **Interoperability & APIs**: Structural "APIs" or organizational patterns used to ensure compatibility or developer familiarity are generally not copyrightable (supported by major rulings like *Google v. Oracle*). We are building our own engine that *happens to be organized similarly* so it's easier to use.
3.  **Clean Room Implementation**: RPGAtlas contains **0% original code** from the reference engine. Every function, even if named similarly (like `Game_Actor`), is a "clean-room" implementation written from scratch. We are replicating the *behavior* and *modding style*, not stealing the *source data*.

---

## 1. 🏗️ Prototype-Based Architecture (✅ COMPLETE)
**Status**: Finished. The engine has been refactored into modular `Game_*` classes and `DataManager` statics.

### 🧐 Is this a good idea?
**Verdict: Yes.** (Rationale documented above).

*   **Result**: 100% plugin compatibility and monkey-patching are now live and verified by `tests/prototype.test.js`.

---

## 🚀 Further Roadmap Details
Detailed implementation steps for the remaining features can be found in the following documents:

*   **PHP Persistence**: See `php-server-2do.txt` 🐘
*   **3D & Z-Levels**: See `z-level-2do.txt` 🧊
*   **CURSWORD Entity**: See `cursword-2do.txt` ⚔️

## 2. 🐘 PHP Data Persistence
**The Goal**: Use PHP to read/write project JSON files instead of relying purely on `localStorage` or local Python servers.

*   **What it takes**: Creating a small PHP backend (e.g., `api.php`) that handles `POST` requests to save project data to disk.
*   **When to start**: Immediately! This can be an **optional feature**.
*   **Safe Path**: Add a setting in the Editor to toggle between "Local Storage Mode" and "Server Mode". The editor would use `fetch()` to talk to the PHP script if enabled. This won't break the browser-only functionality for users without PHP.

## 3. 🧊 3D Z-Levels & Advanced Viewing
**The Goal**: Introduce verticality (Z-axis) and dynamic viewing modes to both the Editor and Player.

*   **Z-Level Features**: 
    1.  Multi-level maps and bridges.
    2.  Z-axis collision and "Functional Z" movement.
*   **Viewing & POV Modes**:
    1.  **2D/3D Toggle**: Switch between classic overhead and a projected 3D perspective.
    2.  **Dynamic Cameras**: Support for **First-Person**, **Third-Person**, and **Free-Cam (Overhead)** POV.
    3.  **Mini-Map**: A UI component for orientation in complex 3D environments.
*   **What it takes**: 
    1. Updating the Map schema for `z` properties.
    2. A rendering engine capable of perspective projection.
    3. Camera state management in `js/engine.js`.
*   **Safe Path**: Start with "Visual Z" (depth sorting) before building the full 3D projection and camera controls.

## 4. ⚔️ CURSWORD: The Master Cursor
**The Goal**: A powerful debug and utility entity that bridges the gap between the Editor and Live Play.

*   **Capabilities**:
    1.  **Free Movement**: Can fly over obstacles to inspect any part of the map.
    2.  **Character Possession**: Can "control" any NPC or Actor as if they were the main character for testing.
    3.  **Event Management**: Used in the editor to select, copy, store, and place events visually.
*   **Safe Path**: Implement as a special "Global Event" that only exists when a debug flag is active, ensuring it never interferes with standard player gameplay.

---

*“Chart your world. Tell your story.”* — **RPGAtlas Team** 🧭
