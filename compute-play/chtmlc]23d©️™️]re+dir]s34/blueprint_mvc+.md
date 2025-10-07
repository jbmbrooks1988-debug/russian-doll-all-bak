# Blueprint for MVC+ Architecture

This document outlines the refactoring of the C-HTML application from a file-based (CSV) module system to a more robust Inter-Process Communication (IPC) model using pipes.

## 1. Core Architectural Changes

The core of the refactor is to move away from `system()` calls and file I/O for communication between the main application and game modules. The new architecture will treat game modules as true child processes, managed by `model.c`, with communication handled via `stdin` and `stdout` streams redirected through pipes.

## 2. Component Responsibilities

### `model.c` - The Data & Logic Hub
- **Process Management:** `model.c` will be responsible for spawning the game module (e.g., `game.c`) as a child process using `fork()` and `execv()`.
- **IPC Setup:** It will create two pipes for bidirectional communication:
    1.  **`input_pipe` (Parent -> Child):** The model will write user input from the controller to this pipe, which will serve as the `stdin` for the game module.
    2.  **`output_pipe` (Child -> Parent):** The model will read game state and rendering data from this pipe, which is fed by the `stdout` of the game module.
- **Data Primitives:** It will define the core data structures for game state representation. These primitives will be used for communication between the model and the view.
    - **Render Primitives:** Structs for basic shapes that the view can render directly.
        - `typedef struct { int id; float x, y, z; float size; float color[4]; } Shape;` (e.g., Circle, Square, Cube, Sphere).
    - **Game Logic Primitives:** Structs for game objects.
        - `typedef struct { int id; int health; int score; int shape_id; } GamePiece;`
- **State Management:** The model will maintain the canonical game state. It will update this state based on the data received from the game module's pipe and make it available for the view to render.

### `view.c` - The "Dumb" Renderer
- **Responsibility:** The view's only job is to render the current game state as provided by the model.
- **No More Logic:** All file I/O (`fopen`, `fscanf`) and game logic for positioning elements from `output.csv` will be removed.
- **Rendering Loop:** The `canvas_render_sample` function will be replaced by a generic `render_canvas` function. This function will:
    1.  Get the list of `Shape` primitives from the model.
    2.  Iterate through the list and draw each shape on the screen using OpenGL.

### `controller.c` - The Input Forwarder
- **Responsibility:** Capture user input and forward it to the model.
- **Input Handling:** It will capture keyboard and mouse events.
- **Serialization:** It will serialize these events into a simple, well-defined string format (e.g., `KEY_UP`, `MOUSE_CLICK 120,340`).
- **Forwarding:** The serialized input string will be passed to a function in the model (e.g., `model_send_input(char* input)`), which will then write it to the `input_pipe` for the game module to consume.

### `module/game.c` - The Game Logic Engine
- **Standalone Executable:** The game will be compiled as a separate executable.
- **Main Loop:** It will run in an infinite loop, performing three steps:
    1.  **Read Input:** Read a command from `stdin` (the pipe from the model).
    2.  **Update State:** Update its internal game state based on the command (e.g., move the player, check for collisions).
    3.  **Write Output:** Serialize the entire visible game state (e.g., the positions and types of all objects) into the defined protocol and write it to `stdout` (the pipe to the model).

## 3. Communication Protocol

A simple, line-based text protocol will be used for communication through the pipes.

-   **Controller -> Model -> Game (Input Pipe):**
    -   `UP`
    -   `DOWN`
    -   `LEFT`
    -   `RIGHT`

-   **Game -> Model (Output Pipe):**
    -   A list of primitives to be rendered, separated by semicolons.
    -   `[TYPE],[x],[y],[size],[r],[g],[b];[TYPE],[x]...`
    -   **Example:** `SQUARE,10,20,15,1.0,0.0,0.0;CIRCLE,50,60,10,0.0,1.0,0.0`
    -   This string represents a red square and a green circle. The model will parse this and update its list of renderable shapes.

## 4. Example Workflow: Player Moves Right

1.  User presses the right arrow key.
2.  `controller.c` captures the `GLUT_KEY_RIGHT` event.
3.  Controller calls `model_send_input("RIGHT")`.
4.  `model.c` writes the string `"RIGHT
"` to the `input_pipe`.
5.  `game.c` reads `"RIGHT
"` from its `stdin`.
6.  The game logic updates the player's internal X-coordinate.
7.  `game.c` writes the new scene description to its `stdout`. For example: `"SQUARE,20,20,15,1.0,0.0,0.0"`.
8.  `model.c` reads the string from the `output_pipe`.
9.  The model parses the string and updates its internal `Shape` data structure for the player.
10. The main loop calls `display()`, which tells `view.c` to render.
11. `view.c` gets the updated shape list from the model and draws the square at the new position (20, 20).
