# WSR - World Stock Market Simulation

WSR is a turn-based stock market simulation game where you can play against computer opponents to build your wealth.

## For Users

### Compilation

To compile the game and all its modules, run the compile script:

```bash
./#.compile.sh
```

This will create several executables in the root directory, including the main `game` executable.

### How to Play

1.  **Start the game**:
    ```bash
    ./game
    ```

2.  **Game Setup**: At the beginning of a new game, you will be prompted to set up the game:
    *   Enter the number of players (1-5).
    *   Enter the game length in years.
    *   Enter the starting cash amount.
    *   For each player, enter a name and type (Human or Computer).

3.  **Gameplay**: The game proceeds in turns, where each year is a turn. During your turn, you will be presented with a main menu with many options to manage your assets, research companies, and perform transactions.

4.  **Saving and Loading**:
    *   **To Save**:
        1.  From the main menu, select option `1. File`.
        2.  In the File Menu, select `1. Save Game`.
        3.  Enter a name for your save file (using alphanumeric characters, dashes, or underscores).
        4.  The game will be saved to a `.zip` file in the `saves/` directory.
    *   **To Load**:
        1.  From the main menu, select option `1. File`.
        2.  In the File Menu, select `2. Load Game`.
        3.  A list of available saves will be displayed.
        4.  Enter the number corresponding to the save you want to load.
        5.  The game will automatically load the selected state and continue from the saved year.

## For Developers

### Project Structure

*   `src/main.c`: Contains the main game loop and core logic.
*   `src/file_submenu.c`: A separate module for handling save and load functionality.
*   `setup_corporations.c`, `setup_governments.c`: Scripts to initialize game data.
*   `ai_module.c`, `research_report.c`, `db_search.c`: Other standalone game modules.
*   `corporations/`, `governments/`: These directories contain the raw data for corporations and governments, with generated data being placed in their respective `generated` subdirectories.
*   `saves/`: This directory stores the `.zip` files for saved games.
*   `#.compile.sh`: The main build script that compiles all C source files into separate executables.
*   `rules.txt`: Contains the architectural rules for the project.

### Architecture

This project follows a modular architecture where different functionalities are implemented as separate, standalone executables.

*   **No direct linking**: C source files are not linked together. Instead, the main `game` process uses the `system()` call to execute other modules (e.g., `file_submenu`, `ai_module`).
*   **Data Sharing via Files**: Modules share data through external text files. For example, the main game state is passed to the save module via the `gamestate.txt` file. This design choice is for better data visualization and modularity.

### Save/Load Mechanism

The save and load mechanism is a good example of the project's architecture in practice.

1.  **Saving**:
    *   The main `game` process, at the beginning of each human player's turn, saves the entire game state (players, market, companies, game progress) into `gamestate.txt`.
    *   When the user chooses to save, the `file_submenu` executable is called.
    *   `file_submenu` takes the `gamestate.txt` file, along with the `corporations/generated/` and `governments/generated/` directories, and archives them into a new `.zip` file in the `saves/` directory.

2.  **Loading**:
    *   When the user chooses to load, the `file_submenu` executable displays the available save files.
    *   It then unzips the selected save file, which overwrites the current `gamestate.txt` and the data in `corporations/generated/` and `governments/generated/`.
    *   After successfully preparing the files, `file_submenu` creates a signal file named `.load_successful`.
    *   The main `game` process detects this signal file upon the completion of `file_submenu`. It then calls its internal `load_game_state()` function, which parses `gamestate.txt` and re-initializes all game variables.
    *   Finally, the main game loop is restarted from the loaded year, allowing the user to seamlessly continue their game.
