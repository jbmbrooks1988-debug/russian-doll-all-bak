# Blueprint: CHTMGL Alpha Integration
**Status:** WORKING DRAFT
**Reference:** `glos-tpmos-dev.md` (Phase 1)

---

## 1. Objective
Enable structural UI parsing and rendering in GL-OS. This will allow projects to define layouts using nested tags like `<window>`, `<panel>`, and `<div>`.

## 2. Structural Changes

### 2.1 Parser (`gltpm_parser.c`)
- **Node-Based Parsing:** Instead of a flat list, the parser should maintain a stack of active nodes to handle nesting.
- **New Tags:**
    - `<panel x="..." y="..." width="..." height="..." color="..." />`
    - `<div>` (Viewport container)
    - `<ul>`/`<li>` (Layout list)
    - `<canvas id="..." />` (3D/2D Viewport)
- **Attribute Inheritance:** Child nodes (buttons, text) should inherit or be offset by their parent's coordinates.

### 2.2 Scene Structure (`gl_desktop.c`)
Extend `GLTPMScene` to include a recursive node list:

```c
typedef struct CHTMLNode {
    char tag[32];
    float x, y, w, h;
    float color[4];
    char id[64];
    char label[128];
    char onClick[128];
    struct CHTMLNode *children;
    int child_count;
} CHTMLNode;

// Update GLTPMScene
typedef struct {
    // ... existing ...
    CHTMLNode root;
} GLTPMScene;
```

### 2.3 Renderer (`gl_renderer.c` / `gl_desktop.c`)
- **Recursive Render Loop:** A function `render_node(CHTMLNode *node)` that handles drawing the node and its children.
- **Scissor Clipping:** Use `glScissor` for `<div>` and `<panel>` to ensure children don't bleed out.
- **Canvas Hook:** When a `<canvas>` tag is encountered, invoke the specific renderer (e.g., `draw_voxel_editor`).

### 2.4 Hybrid IPC Implementation
- **Standard Mode (Default):** 
    - Parser monitors `state.txt` for variable updates.
    - Controller appends `COMMAND:handler` to `history.txt` on click.
- **High-Speed Mode (Configurable Toggle):** 
    - When `g_high_speed_ipc_enabled` is true, GL-OS attempts to open SHM/pipes for sub-16ms communication.
    - Implement `shmem_manager` to read `SHAPE` and `CANVAS` events from shared memory blocks.
    - Scene updates are triggered by `frame_changed.txt` size changes.

---

## 3. Implementation Steps
1.  **Tag Registration:** Add string detection for the new tags in `gltpm_load_scene`.
2.  **Coordinate Math:** Implement a "Current Offset" stack in the parser so that `<button x="10" />` inside a `<panel x="100" />` renders at `x=110`.
3.  **Proof of Concept:** Update `projects/chtmgl-alpha/layouts/index.chtmgl` to use a nested `<panel>` and verify it renders correctly in GL-OS.

---

## 4. Verification KPIs
- **Nesting Depth:** Support at least 3 levels of nesting (`window > panel > div > button`).
- **Layout Parity:** The GL-OS render must visually match the intended layout described in the `.chtmgl` file.
