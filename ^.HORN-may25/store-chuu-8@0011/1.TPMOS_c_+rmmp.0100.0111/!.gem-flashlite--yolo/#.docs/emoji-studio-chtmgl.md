# Blueprint: Emoji Studio CHTML Refactor
**Status:** SPECIFICATION
**Reference:** `glos-tpmos-dev.md` (Phase 2)

---

## 1. Objective
Refactor the standalone Emoji Studio into a sovereign GL-OS application by replacing all hardcoded OpenGL UI logic with CHTMGL markup. This serves as the pilot for "UI Sovereignty."

## 2. Layout Structure (`layouts/main.chtml`)
The UI will be defined entirely in markup:

```html
<window title="Emoji Studio v2.0" sovereign="true">
    <!-- Top Bar: Navigation & Tools -->
    <panel id="toolbar" height="10%">
        <button label="Save" onClick="SET_ACTION:SAVE_EMOJI" />
        <button label="Load" onClick="SET_ACTION:LOAD_EMOJI" />
        <button label="Exit" onClick="BACK" />
    </panel>

    <!-- Center: The Voxel Editor (Hardware Accelerated) -->
    <div id="editor-container" height="70%">
        <canvas id="voxel_editor" 
                source="projects/emoji-studio/pieces/editor" 
                mode="3D_VOXEL" />
    </div>

    <!-- Bottom: The Emoji Picker (Horizontal Carousel) -->
    <div class="thumbnail-viewport" height="20%">
        <ul class="thumbnail-list" orientation="horizontal">
            <!-- Dynamically populated by manager -->
            ${emoji_list_items}
        </ul>
    </div>
</window>
```

## 3. Technical Implementation

### 3.1 Parser/Renderer Updates
- **`<div>` Viewport:** The renderer must set up a `glViewport` and scissor box based on the `div` bounds.
- **`<ul>` Horizontal Layout:** Implement a layout engine that calculates horizontal offsets for child `<li>` elements.
- **`<canvas>` Integration:** The `voxel_editor` canvas will receive direct input events (mouse delta for rotation, clicks for voxel placement) routed from `controller.c`.

### 3.2 Manager IPC (`manager/emoji-studio_manager.c`)
- **Standard IPC (File-Based):** 
    - Manager watches `pieces/` for new assets and updates `state.txt` for the `${emoji_list_items}` substitution.
- **High-Performance IPC (SHM):**
    - Uses the `SHAPE` protocol for real-time voxel updates in the `<canvas id="voxel_editor">`.
    - `CANVAS:voxel_editor:<x>,<y>` events are sent via SHM for zero-latency voxel placement.

## 4. Verification KPIs
- **Zero GL Calls in Host:** `emoji-studio_host.c` must contain NO manual `glBegin/glEnd` or UI drawing logic.
- **Scroll Fluidity:** The horizontal picker must maintain 60FPS during interaction.
- **Focus Integrity:** Clicking the canvas must not lose focus from the global GL-OS menu system.
