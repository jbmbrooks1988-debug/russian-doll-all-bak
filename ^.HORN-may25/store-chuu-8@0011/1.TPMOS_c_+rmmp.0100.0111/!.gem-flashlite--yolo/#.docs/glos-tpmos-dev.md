# GL-OS & TPMOS Development Guide (m31 Consolidated)
**Status:** AUTHORITATIVE ROADMAP & TECHNICAL SPECIFICATION
**Date:** May 31, 2026

---

## 1. Executive Vision: UI Sovereignty
The goal is to move beyond simple project loading to **UI Sovereignty**. Projects must render rich, hardware-accelerated interfaces using the **CHTMGL (CHtml)** framework within the sovereign bounds of the GL-OS viewport.

### 1.1 Integration Sequence
1.  **CHTMGL Core Integration:** Upgrade `gltpm_parser.c` and `gl_renderer.c` to support structural markup.
2.  **Emoji Studio Refactor:** Pilot the CHTML paradigm by migrating Emoji Studio to a `<canvas>` and `<div>` based architecture.
3.  **P2P-GLOS Forum:** Implement a sovereign, media-rich P2P chat application using the mature CHTMGL stack and `p2p-net`.

### 1.2 Hybrid IPC Architecture
To maintain "One File, One Truth" while achieving high performance, CHTMGL utilizes a dual-layered IPC:
-   **Standard Layer (File-Based / Default):** Uses `history.txt` for input relay and `state.txt` for variable substitution (e.g., `${score}`). This is the default mode for all modules to ensure persistence and auditability.
-   **Performance Layer (SHM/Pipes / Configurable):** Enables high-speed communication via Shared Memory for `<canvas>` rendering and real-time voxel manipulation. This mode is toggled via the `g_high_speed_ipc_enabled` setting in GL-OS.

---

## 2. Phase 1: CHTMGL Integration (The Foundation)
Upgrade the OpenGL parser and renderer to translate HTML-like markup into `GLTPMScene` objects.

### Technical Requirements
-   **Structural Tags:** Implement support for `<window>`, `<panel>`, `<div>` (viewports), and `<ul>`/`<li>` (list/carousel).
-   **The Canvas Tag:** Implement `<canvas id="..." />` for direct 2D/3D viewport injection.
-   **Interaction Routing:** Map GUI events (clicks, scrolls) to the manager's IPC handlers.

---

## 3. Phase 2: Emoji Studio Refactor (The Pilot)
Migrate the standalone Emoji Studio into a fully integrated GL-OS application.

### Implementation Roadmap
-   **Layout Migration:** Create `projects/emoji-studio/layouts/main.chtml`.
-   **Markup UI:** Use `<div class="thumbnail-viewport">` for the picker and `<canvas id="editor_viewport">` for the voxel editor.
-   **Zero-Hardcode Mandate:** Remove manual OpenGL drawing from `emoji-studio_host.c`; delegate all rendering to the CHTMGL parser/renderer loop.

---

## 4. Phase 3: P2P-GLOS Chat Forum (The Capstone)
A sovereign application for real-time, media-rich P2P communication.

### Technical Requirements
-   **Network:** Integrate `p2p-net` transport into the application manager.
-   **Media Sovereignty:** Support `<chat-stream>` and `<media-embed>` tags for inline rendering of images/video/audio.
-   **Security:** Enforce asset sanitization and hash verification for all incoming P2P media.

---

## 5. Technical TODO Checklist

### [ ] Infrastructure (CHTMGL Alpha)
- [ ] Refactor `gltpm_parser.c` for structural node nesting.
- [ ] Implement `gl_renderer.c` handler for `<canvas>` viewports.
- [ ] Add `SLIDER` and `EVENT` IPC message support to `controller.c`.

### [ ] Emoji Studio Migration
- [ ] Establish `project.pdl` with `SOVEREIGN | true`.
- [ ] Port 3D Voxel rendering logic into the CHTMGL `<canvas>` pipeline.
- [ ] Verify horizontal scrolling picker via `<ul>`/`<li>` tags.

### [ ] P2P-GLOS Implementation
- [ ] Implement `chat_manager.c` with `p2p-net` hooks.
- [ ] Create `layouts/main.chtmgl` with dynamic `chat-stream`.
- [ ] Verify P2P message propagation and media-embed rendering.

---

## 6. Key Performance Indicators (KPIs)
-   **Latency:** Input-to-Render latency must remain < 16ms (Mode 1).
-   **CPU Safety:** Idle modules must throttle to < 5% CPU usage (Pulse Discipline).
-   **Sovereignty:** Zero cross-pollination between project state directories.
-   **Stability:** Zero orphaned processes on project exit (SIGTERM Handlers).

---
*"One File, One Truth. The UI is the Performance; the PDL is the Script."*
