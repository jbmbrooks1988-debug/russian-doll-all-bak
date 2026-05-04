# PyOS-TPM Web Edition

A True Piece Method (TPM) compliant virtual desktop environment built with HTML, CSS, JavaScript, and PHP.

## Quick Start

### Option 1: PHP Built-in Server (Recommended)

```bash
cd py2php_os
php -S localhost:8000
```

Then open http://localhost:8000 in your browser.

### Option 2: Any Web Server

Copy the `py2php_os` folder to your web server's document root and access it via browser.

**Note:** PHP is required for file read/write operations (piece persistence, ledger logging).

## Project Structure

```
py2php_os/
├── index.html              # Main desktop container
├── css/
│   ├── desktop.css         # Desktop environment styles
│   └── windows.css         # Window chrome and controls
├── js/
│   ├── desktop.js          # Main desktop initialization
│   ├── window-manager.js   # Window creation/management
│   ├── piece-manager.js    # TPM piece management
│   ├── master-ledger.js    # Audit trail system
│   └── plugin-interface.js # Base plugin class
├── php/
│   └── file_io.php         # File read/write API
└── pieces/                 # TPM piece definitions
    ├── desktop/
    ├── terminal/
    ├── keyboard/
    └── master_ledger/
```

## Features (Phase 1)

### ✅ Desktop Environment
- Gradient background desktop
- Toolbar with File, Apps, and Terminal buttons
- Right-click context menu
- Taskbar with window management
- Real-time clock

### ✅ Window Management
- Create windows dynamically using innerHTML
- Drag windows by title bar
- Resize from bottom-right corner
- Minimize, maximize, close buttons
- Window focus/z-index management
- Taskbar integration

### ✅ TPM Architecture
- **Piece Manager**: Load/save piece state
- **Master Ledger**: Central audit trail for all operations
- **Plugin Interface**: Base class for extensible plugins
- **Event System**: Input → History → Ledger → Process flow

### ✅ Applications

#### Terminal
- Command-line interface in a window
- Built-in commands: `help`, `clear`, `date`, `echo`, `ls`, `pwd`, `whoami`, `apps`, `open [app]`, `version`
- Command history (up/down arrows)
- Auto-scrolling output

#### Demo Counter (TPM Demo App)
- Increment/Decrement/Reset buttons
- Real-time TPM state persistence
- Event log showing all operations
- Demonstrates full TPM architecture

### 🚧 Coming Soon (Phase 2+)
- 🐾 FuzzPet - Virtual pet game
- 🎮 Hangman game
- 🗺️ 2D PyBoard voxel viewer
- 🌍 3D PyBoard voxel viewer (WebGL)
- 🎲 3D Cube visualization
- ⚙️ Enhanced settings
- 🏪 App store
- 👛 Wallet application
- 🔌 More plugins

| Shortcut | Action |
|----------|--------|
| Ctrl+T | Open Terminal |
| Ctrl+W | Close active window |
| Escape | Hide dropdowns/menus |

## TPM Components

### Pieces
Pieces are the fundamental building blocks following the True Piece Method:

- **desktop**: Manages the desktop environment
- **terminal**: Terminal application state
- **keyboard**: Keyboard input handling

### Master Ledger
All state changes and events are logged to the master ledger for full auditability:
- StateChange: When piece state changes
- EventFire: When events are triggered

### Plugin System
Plugins extend functionality by implementing the PluginInterface:
- `initialize()`: Called when plugin loads
- `activate()`: Called when plugin activates
- `deactivate()`: Called when plugin deactivates
- `update(dt)`: Called every frame
- `handleInput()`: Handle user input

## API Reference

### WindowManager

```javascript
// Create a new window
windowManager.createWindow(title, contentHTML, width, height, options);

// Close a window
windowManager.closeWindow(windowId);

// Minimize a window
windowManager.minimizeWindow(windowId);

// Maximize a window
windowManager.maximizeWindow(windowId);
```

### PieceManager

```javascript
// Get piece state
pieceManager.getPieceState(pieceId, key);

// Update piece state
await pieceManager.updatePieceState(pieceId, newValues);

// Fire an event
await pieceManager.fireEvent(pieceId, eventType, eventData);
```

### MasterLedger

```javascript
// Log an entry
masterLedger.log(entryType, message, source);

// Get entries with filters
masterLedger.getEntries({ type: 'StateChange', source: 'desktop' });

// Get latest entries
masterLedger.getLatest(10);
```

## PHP File I/O API

### Read
```
GET /php/file_io.php?action=read&path=pieces/desktop/piece.json
```

### Write
```
POST /php/file_io.php?action=write
Body: { "path": "pieces/desktop/piece.json", "content": "..." }
```

### Append
```
POST /php/file_io.php?action=append
Body: { "path": "pieces/master_ledger/ledger.json", "data": {...} }
```

## Coming Soon (Phase 2+)

- 🎮 Hangman game
- 🗺️ 2D PyBoard voxel viewer
- 🌍 3D PyBoard voxel viewer (WebGL)
- 🎲 3D Cube visualization
- ⚙️ Enhanced settings
- 🏪 App store
- 👛 Wallet application
- 🔌 More plugins

## License

Part of the TPM_OS project.
