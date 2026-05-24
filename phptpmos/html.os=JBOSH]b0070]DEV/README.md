# Mars Streetrace Raider

## Overview

Mars Streetrace Raider is a terminal-style, web-based stock market and business simulation game set on Mars. The game is built with HTML, JavaScript, and a PHP backend that uses a flat-file (`.txt`) system for data persistence. This allows for a lightweight, server-based game that can be run locally.

The game also features a "data twin" relationship with a related project, `3d.tactics.board.frame.php`, sharing core game state data like the in-game clock (`wsr_clock.txt`) to ensure a cohesive universe.

## Current Architecture

The application currently runs on a simple PHP built-in web server.
-   **Frontend:** Plain HTML, CSS, and JavaScript files that are served directly to the browser.
-   **Backend:** A collection of individual `.php` scripts inside the `/php` directory. Each script handles a single function, like fetching game state, creating a player, or setting up corporations by reading from and writing to `.txt` files in the `/data` directory.
-   **Data:** Game state, player data, corporation details, and more are all stored in human-readable `.txt` files.

## Refactoring and Modernization Plan

The goal is to unify the Mars Streetrace Raider (`@msr]PHP]dec27]b1]PURE/`) and 3D Tactics Board Game (`@3d.tactics.board.frame.php]d5/`) projects into a single, cohesive application. This will be achieved by establishing `@g.clay]b3.txt.html` as the main "desktop" environment, capable of launching each game as a separate, windowed application.

This refactoring will use the architectural pattern already established by `craft.js` and `script.js`, where applications are JavaScript classes that create their UI using `innerHTML` within a draggable window.

### Phase 1: Establish the Main Desktop Environment

The first step is to designate and prepare `@g.clay]b3.txt.html` as the primary entry point for the entire unified application.

1.  **Set `g.clay]b3.txt.html` as the Main `index.html`:**
    *   This file, with its associated CSS and the logic from `script.js`, will serve as the "Operating System" shell. It already provides the necessary desktop features: a toolbar, context menus, and a window manager.
2.  **Verify and Consolidate Scripts:**
    *   Ensure that the main `index.html` correctly loads all necessary application scripts, including `script.js` (for window management), `msr.js`, `craft.js`, and a new `tactics.js` (to be created in Phase 2).
    *   Confirm that the `processCommand` function in `script.js` can launch the `craft` application as a test case.

### Phase 2: Convert and Integrate Applications

The next step is to convert the existing MSR and 3D Tactics HTML pages into self-contained JavaScript modules that can be launched as "apps" by the main desktop.

1.  **Convert MSR into `MsrApp.js`:**
    *   Refactor the `@msr]PHP]dec27]b1]PURE/index.html` file into a JavaScript class named `MsrApp` (likely within `msr.js`).
    *   This class's `showGUI()` method will create a new window and inject the entire MSR game interface into it using `innerHTML`.
    *   All associated JavaScript logic (data fetching from the PHP backend, UI updates, etc.) will be encapsulated within this class to ensure it functions correctly within its window.
2.  **Convert 3D Tactics into `TacticsApp.js`:**
    *   Similarly, refactor the `@3d.tactics.board.frame.php]d5/index.html` file into a new `TacticsApp.js` class.
    *   This class will also have `showGUI()` and `closeGUI()` methods that manage its window and `innerHTML`.
    *   The Three.js logic from its current `js/main.js` will be encapsulated within this new class.
3.  **Update the Desktop `processCommand`:**
    *   Add new commands to the `processCommand` function in `script.js` (e.g., `msr` and `tactics`).
    *   These commands will call `window.msrApp.showGUI()` and `window.tacticsApp.showGUI()` respectively, allowing users to launch the games from a terminal window within the main desktop environment.

This plan ensures a unified application that allows multiple games and tools to run simultaneously, all while using the established and proven architecture.


2do NOW! : 
✦ I see the problem exactly. It's a classic singleton vs. instance issue.

  The Problem:

  You're right, the current system is designed to use only one "instance" of each
  application. When craft.js loads, it creates a single, global object called
  window.craftApp. Every time you run the craft command in any terminal, it just
  calls showGUI() on that same, single object. If the app's window is already
  open, it doesn't create a new one. This is not like a real OS where you can
  open multiple copies of the same program.

  The Fix:

  To fix this, we need to refactor the application architecture to be
  instance-based. Here is the plan, which we will execute first as you requested:

   1. Make `CraftApp` Instantiable: We will modify the CraftApp class so that
      every time we create a new CraftApp(), it generates a completely new,
      independent version of the craft game.
       * Each instance will get a unique ID (e.g., craft_instance_12345).
       * The innerHTML that builds the app window will be updated to use this
         unique ID for all its internal elements, preventing ID conflicts between
         different craft windows.

   2. Create a Global App Registry: We'll introduce a global object (e.g.,
      window.appInstances = {}) that will act as a "process manager". Every time
      a new app instance is created, it will register itself in this object with
      its unique ID.

   3. Update Event Handlers (`onclick`): The onclick buttons inside the app
      window (like the close button) will be modified to use this new registry to
      call functions on the correct instance. For example, instead of
      craftApp.closeGUI(), it will be
      window.appInstances['craft_instance_12345'].closeGUI().

   4. Proper Cleanup: When you close an application window, it will now do two
      things:
       * Remove its HTML from the page completely.
       * Remove itself from the global appInstances registry, freeing up that
         "process".

   5. Update the Terminal Command: Finally, we'll change the craft command in
      script.js. Instead of just showing the one global app, it will now create a
      new CraftApp() every single time it's run, which will in turn create a
      brand new window and game instance.

  This will give you the behavior you expect from a desktop OS.

  I will now begin implementing this fix, starting with craft.js.

