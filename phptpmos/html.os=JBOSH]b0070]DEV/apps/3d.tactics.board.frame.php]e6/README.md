# 3D Tactical Board Game

A simple 3D tactical game built with HTML/JS and Three.js featuring colored cubes as game pieces.

## Features

- Green cubes represent ground/grass
- Purple cubes represent soldiers
- Blue cubes represent water
- Yellow selector highlights valid cubes
- Control system with Ctrl key to toggle between selector and piece movement
- Arrow keys to move selector or selected piece

## Controls

- **Arrow Keys**: Move selector or selected piece
- **Ctrl**: Toggle between selector mode and piece movement mode
- When in selector mode: Arrow keys move the yellow selector
- When a piece is selected: Arrow keys move the selected piece

## Setup

To run the game:

1. Clone or download the project
2. Serve the files using a local web server (the game uses Three.js which requires a web server)
3. Open `index.html` in your browser

For example, you can use Python's built-in server:
```bash
python3 -m http.server 8000
```

Then navigate to `http://localhost:8000` in your browser.

## Project Structure

- `index.html`: Main HTML file with the game container and controls info
- `js/main.js`: Main JavaScript file containing the game logic