
# OpenGL Tactical Game

A simple tile-based tactical game built in C with OpenGL and GLUT.

## How to Play

1.  **Compile the game:**
    Run the `compile.sh` script to compile the main game and the AI module.
    ```bash
    ./compile.sh
    ```

2.  **Run the game:**
    ```bash
    ./game
    ```

3.  **Gameplay:**
    *   The game is turn-based. Player 1 (you) starts.
    *   Click on one of your pieces to open a menu.
    *   **Show Stats:** Click "Show Stats" to see the HP and Attack of your piece.
    *   **Move:** Click "Move" to see the movement range of your piece. Click on a highlighted tile to move your piece.
    *   After you move, the AI (Player 2) will take its turn.

## For Developers

This project uses a modular, file-based architecture.

### Code Structure

*   **`main.c`:** The main game client. It handles rendering, user input, and the main game loop.
*   **`ai_module.c`:** The AI logic for Player 2. It is a separate executable that is called by the main game.
*   **`assets/`:** Contains all the game assets (sprites and backgrounds).
*   **`states/`:** Contains the state of each piece in separate text files.
    *   `state_<x>_<y>.txt`: Stores the stats (HP, attack, etc.) for the piece at coordinates (x, y).
    *   `weights.template.txt` and `biases.template.txt`: Template files for the AI's decision-making parameters.

### AI System

The AI is designed to be modular and customizable through files.

*   **Communication:** The main game and the AI module communicate via files:
    1.  The main game writes the current board state to `board_state.txt`.
    2.  The main game calls the `ai_module` executable.
    3.  The `ai_module` reads `board_state.txt`, decides on a move, and writes the move to `ai_move.txt`.
    4.  The main game reads `ai_move.txt` and executes the move.

*   **Customizing the AI:**
    *   The AI's decision-making is based on a weighted system. The weights and biases for each piece are stored in `weights.<piece>.txt` and `biases.<piece>.txt` files (not yet fully implemented).
    *   You can "train" the AI by adjusting the values in these files.
