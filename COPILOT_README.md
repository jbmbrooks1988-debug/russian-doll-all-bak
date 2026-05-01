# TPMOS to PHP/HTML GL-OS Web Desktop - Conversion Plan

## Project Scope (CRITICAL!)

**This project is NOT a full TPMOS conversion.** We're implementing:

✅ **GL-OS Desktop (Web Version)** - Graphical desktop environment in HTML5/PHP  
✅ **Terminal Windows** - Command-line terminals running INSIDE the graphical desktop  
✅ **File-backed State** - PHP reads/writes TPMOS piece files  
✅ **Real-time Updates** - WebSocket or polling for live frame updates  

❌ NOT porting full C codebase  
❌ NOT converting all projects/games  
❌ NOT full TPMOS parser pipeline  
❌ NOT every system component—just GL-OS visual shell  

---

## Why GL-OS for Web?

**GL-OS is the ideal target because:**
- It's already **isolated as an APP** (not a project)
- It has **clear visual semantics**: windows, desktop, viewport
- It's **view-agnostic**: can render in OpenGL, ASCII, or HTML5
- HTML5 Canvas/DOM can **easily replicate 3D viewport + terminal overlay**
- PHP **naturally handles file I/O** for state management
- **Browser-based** makes it cross-platform (Windows, Mac, Linux)

---

## Architecture Overview

### High-Level Flow

```
┌─────────────────────────────────────────────────────────────┐
│                      BROWSER (HTML5)                        │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  GL-OS Web Desktop (Canvas/DOM)                       │  │
│  │  ├── Desktop View (3D perspective or 2D isometric)    │  │
│  │  ├── Window Manager (draggable, resizable)            │  │
│  │  ├── Terminal Windows (interactive)                   │  │
│  │  │   ├── [Terminal 1] $ █                              │  │
│  │  │   ├── [Terminal 2] $ █                              │  │
│  │  │   └── [Terminal N] $ █                              │  │
│  │  └── Event Handlers (mouse, keyboard)                 │  │
│  └───────────────────────────────────────────────────────┘  │
│                         ↑↓ (WebSocket/AJAX)                 │
└─────────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────────────┐
│                  PHP Backend (Server-side)                  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  GL-OS Manager (PHP)                                  │  │
│  │  ├── Session management (desktop state)               │  │
│  │  ├── Window state (positions, sizes, active window)   │  │
│  │  ├── Terminal multiplexer (PTY + tmux/screen)         │  │
│  │  ├── Frame composition (from TPMOS pieces)            │  │
│  │  └── File I/O (reads/writes TPMOS state)              │  │
│  └───────────────────────────────────────────────────────┘  │
│                         ↓                                    │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  TPMOS File System (Read-only for desktop view)       │  │
│  │  ├── pieces/apps/gl_os/session/                       │  │
│  │  ├── pieces/apps/gl_os/state/                         │  │
│  │  └── pieces/display/current_frame.txt                 │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Technology Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| **Frontend** | HTML5, Canvas/WebGL, JavaScript | Desktop rendering, window manager |
| **Backend** | PHP 7.4+ | File I/O, session management, terminal control |
| **Terminal** | `tmux` or `screen` + pty | Multiple command-line windows |
| **Communication** | WebSocket (or AJAX polling) | Real-time updates between browser and server |
| **Window Manager** | Custom JS library | Draggable, resizable, z-order management |
| **State Storage** | TPMOS file system | `.txt` and `.pdl` files (as seen in C version) |

---

## Core Components to Build

### 1. PHP Backend (`phptpmos/php/`)

#### GL-OS Manager
```php
class GLOSManager {
    // Session state (windows, desktop bg, active window)
    private $session;
    
    // Terminal multiplexer (manage multiple PTY sessions)
    private $terminalMux;
    
    // Window manager (positions, z-order, focus)
    private $windowManager;
    
    // Frame composer (builds desktop view)
    private $frameComposer;
}
```

**Key Methods:**
- `createWindow($type, $x, $y, $width, $height)` - Create terminal/app window
- `closeWindow($windowId)` - Close window and cleanup PTY
- `resizeWindow($windowId, $width, $height)` - Resize and reflow content
- `moveWindow($windowId, $x, $y)` - Update z-order as needed
- `getFrameData()` - Return current desktop state for browser
- `handleInput($windowId, $input)` - Route keyboard input to active terminal

#### Terminal Multiplexer
```php
class TerminalMux {
    // Fork PTY per window (using proc_open + /dev/ptmx on Linux)
    public function createTerminal($windowId);
    
    // Read PTY output, convert ANSI → HTML/JSON
    public function readTerminal($windowId);
    
    // Write input to PTY (keyboard events from browser)
    public function writeTerminal($windowId, $input);
    
    // Cleanup on close
    public function closeTerminal($windowId);
}
```

#### Window Manager
```php
class WindowManager {
    // Track window positions, sizes, z-order
    private $windows = [];
    
    // Serialize to JSON for browser
    public function getWindowState();
    
    // Update on user drag/resize
    public function updateWindowGeometry($windowId, $x, $y, $w, $h);
}
```

### 2. Frontend (HTML5/JavaScript) - `phptpmos/js/`

#### Desktop Canvas/DOM
```javascript
class GLOSDesktop {
    constructor(canvas, wsUrl) {
        this.canvas = canvas;
        this.ws = new WebSocket(wsUrl);
        this.windows = [];
    }
    
    // Render desktop + all windows
    render(frameData) {
        // 1. Draw background (3D perspective or 2D)
        // 2. Draw window frames (title bar, borders)
        // 3. Draw terminal content (ANSI→CSS conversion)
    }
    
    // Window manager
    createWindow(type, pos);
    closeWindow(windowId);
    dragWindow(windowId, newPos);
    resizeWindow(windowId, newSize);
}
```

#### Terminal Emulator (in-browser)
```javascript
class TerminalWindow {
    constructor(windowId, width, height) {
        this.buffer = []; // Line buffer for terminal
        this.cursor = {x: 0, y: 0};
        this.ansiParser = new ANSIParser();
    }
    
    // Update from PTY output
    updateContent(ptyOutput);
    
    // Handle Ctrl+C, arrow keys, etc.
    handleInput(key);
}
```

#### WebSocket Handler
```javascript
ws.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    
    if (msg.type === 'frame') {
        // Update desktop view
        desktop.render(msg.frameData);
    } else if (msg.type === 'terminal_output') {
        // Update specific terminal window
        getWindow(msg.windowId).updateContent(msg.output);
    }
};
```

---

## Implementation Roadmap

### Phase 1: Foundation (Weeks 1-2)
- [ ] Setup PHP project structure (`phptpmos/php/`)
- [ ] Setup JS/HTML project structure (`phptpmos/js/`)
- [ ] Create GLOSManager skeleton
- [ ] Implement basic WebSocket communication
- [ ] Load GL-OS session state from TPMOS files

### Phase 2: Window Management (Weeks 3-4)
- [ ] Implement WindowManager (track position, size, z-order)
- [ ] Create TerminalMux (fork PTY per window)
- [ ] Build browser-side window renderer (Canvas/DOM)
- [ ] Implement drag/resize in browser
- [ ] Sync window geometry server ↔ client

### Phase 3: Terminal Emulation (Weeks 5-6)
- [ ] ANSI parser (convert escape codes to CSS/HTML)
- [ ] PTY input/output routing
- [ ] Keyboard event handling (Ctrl+C, arrow keys, etc.)
- [ ] Multi-window terminal support
- [ ] Line scrolling and history buffer

### Phase 4: Rendering (Weeks 7-8)
- [ ] Canvas renderer for desktop viewport (2D or 3D perspective)
- [ ] Window frame rendering (title bar, borders, shadows)
- [ ] Terminal content rendering inside windows
- [ ] Real-time frame composition
- [ ] Performance optimization (delta updates)

### Phase 5: Integration & Testing (Week 9)
- [ ] Full end-to-end testing (browser → PHP → PTY → shell)
- [ ] Load TPMOS piece files and display in terminals
- [ ] Test with multiple terminal windows
- [ ] Performance benchmarking
- [ ] Documentation

---

## Key Design Decisions

### 1. Terminal Backend: PTY vs Shell Emulation?
- ✅ **Use real PTY** (`/dev/ptmx` on Linux, `conpty` on Windows)
- ✅ **Use tmux/screen** for multiplexing (handles focus, resize, etc.)
- ❌ Avoid custom shell emulation (complexity, limited)

### 2. Frontend: Canvas vs DOM?
- **Canvas** for 3D perspective rendering (isometric/first-person)
- **DOM** for terminal windows (easier text rendering, copy/paste)
- **Hybrid**: Canvas for desktop, embedded DOM for terminal content

### 3. Communication: WebSocket vs AJAX?
- ✅ **WebSocket** for low-latency terminal I/O
- ✅ **Fallback to AJAX** for older browsers or polling
- Use binary frames for efficient ANSI output transfer

### 4. File I/O: Direct vs Cached?
- PHP reads TPMOS files from disk (slower but simpler)
- Cache in PHP session (faster but must invalidate)
- Use `stat()` to detect file changes (like original C version)

---

## Example: A User Opens a Terminal

```
1. User clicks "New Terminal" button in desktop
   ↓
2. Browser sends: {action: "createWindow", type: "terminal", x: 100, y: 100}
   ↓
3. PHP GLOSManager.createWindow()
   - Fork PTY with /bin/bash
   - Create WindowManager entry
   - Write to pieces/apps/gl_os/session/state.txt
   ↓
4. PHP returns: {windowId: "term_001", pty: "/dev/pts/5"}
   ↓
5. Browser creates TerminalWindow object
   - DOM or Canvas element
   - WebSocket listener for PTY output
   ↓
6. User types "ls"
   ↓
7. Browser sends: {windowId: "term_001", input: "l", "s", "\n"}
   ↓
8. PHP TerminalMux writes to PTY
   ↓
9. Shell outputs directory listing
   ↓
10. PHP reads PTY, converts ANSI → JSON, sends via WebSocket
    ↓
11. Browser ANSIParser renders as colored text
    ↓
12. User sees terminal window updated in real-time
```

---

## File Structure (Proposed)

```
/phptpmos/
├── php/
│   ├── src/
│   │   ├── GLOSManager.php
│   │   ├── TerminalMux.php
│   │   ├── WindowManager.php
│   │   ├── FrameComposer.php
│   │   └── ANSIParser.php
│   ├── api/
│   │   ├── websocket.php
│   │   └── rest.php
│   └── config.php
├── js/
│   ├── desktop.js         (GLOSDesktop class)
│   ├── terminal.js        (TerminalWindow class)
│   ├── window-manager.js  (Window drag/resize)
│   ├── ansi-parser.js     (ANSI → CSS)
│   └── main.js            (WebSocket init, event handlers)
├── html/
│   └── index.html         (Desktop canvas + terminal DOM containers)
├── css/
│   └── style.css          (Window styling, terminal theme)
└── README.md              (API docs, dev guide)
```

---

## Next Steps

1. **Read GL-OS docs** from original C codebase
2. **Analyze current GL-OS architecture** (window model, state files)
3. **Decide: Canvas vs DOM** for rendering
4. **Prototype PTY communication** in PHP
5. **Build minimal desktop** (1 terminal window, keyboard input)
6. **Iterate** on UX, performance, feature parity

---

## Questions to Clarify

1. **Desktop rendering**: Isometric 2D, First-person 3D, or simple 2D grid?
2. **Terminal theme**: Dark (Solarized), light, or custom?
3. **Max windows**: Single terminal, or full multi-window desktop?
4. **TPMOS integration**: Display TPMOS apps IN terminal windows, or separate?
5. **Browser support**: Modern (Chrome/Firefox) or legacy (IE11)?

---

**Status:** ✅ Scope clarified | ⏳ Awaiting approval to proceed with Phase 1

