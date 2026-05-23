# Unified File-Ops Reusability Plan: op-ed & agy-text-editor
**Date:** 2026-05-23
**Status:** ARCHITECTURAL SYNERGY

## 1. Objective
To prove the robustness of the TPMOS "Muscle in Ops" model by implementing a unified, reusable file operation system that serves both directory-based projects (`op-ed`) and file-based documents (`agy-text-editor`).

## 2. Project Analysis: Needs & Similarities

| Feature | op-ed (RPG Maker) | agy-text-editor (Text Editor) |
| :--- | :--- | :--- |
| **Atomic Unit** | Directory (Game Project) | File (Text Document) |
| **Sovereign Source** | `project.pdl` (Metadata) | `document.txt` (Working Buffer) |
| **Browser Need** | Directory Browser (Games) | File Browser (All text files) |
| **Persistence** | PDL Sync + Map Directory Copy | Raw Text Read/Write |
| **Handshake** | Pulse Map Palette Refresh | Pulse RAM Buffer Reload |

### Shared Core:
- **Navigation:** Both require a tree-view or list-view to select a target.
- **State Handover:** Both suffer from race conditions where the Manager's internal state (RAM) desyncs from the Disk during the UI transition.
- **Topological Mandate:** Both must use `pieces/ops/file-op/` for all filesystem muscle.

## 3. The "Sovereign Handshake" (Handover Protocol v1.1)

We will standardize the **Double-Marker Handshake** to ensure reusability.

### The Lifecycle:
1. **Trigger:** Manager calls `file_manager.+x` (The Browser).
2. **Operation:** User selects a file/project. `file_manager` calls `load_file_op.+x`.
3. **Write (The Muscle):** `load_file_op` copies data to the project's buffer and updates its sovereign metadata (`project.pdl`).
4. **Pulse (Request):** `load_file_op` appends to `projects/<id>/manager/state_changed.txt`.
5. **Wait (Handshake):** `load_file_op` polls `sync_confirmed.txt` for an acknowledgment.
6. **Sync (Manager):** The Manager (which was in the background) sees the pulse, re-reads the disk truth (PDL or Buffer), and writes to `sync_confirmed.txt`.
7. **Transition:** `load_file_op` sees the Ack and calls `transition_to_layout` to return to the Editor/Game UI.

## 4. Reusable Ops Toolbox

To support both projects, our Ops must be polymorphic:

- **`project_scan_op.+x`**: Lists directories (for `op-ed`) or files (for `agy-text-editor`) based on a `--type` flag.
- **`load_file_op.+x`**:
    - **Mode A (File):** Copies `source.txt` -> `pieces/document.txt`.
    - **Mode B (Dir):** Copies `source_dir/` -> `games/current_game/`.
- **`sync_pdl_op.+x`**: Surgically updates any `key|value` pair in a standard PDL file.

## 5. Development Roadmap: Slicing the Fix

### Slice A: The Sovereign Handshake (Verification)
- Update `load_file_op.c` to include the `sync_confirmed.txt` polling logic.
- Update `agy-text-editor_manager.c` to acknowledge the pulse.
- **Success:** Loading a file in `agy-text-editor` NEVER results in a blank screen or stale header.

### Slice B: Polymorphic Project Loading
- Update `load_file_op.c` to handle directory-to-directory copies for `op-ed`.
- Update `op-ed_manager.c` to call the same `load_file_op.+x` that the text editor uses.
- **Success:** `op-ed` can switch "Games" using the global system browser.

### Slice C: The "Universal Save"
- Implement a `save_file_op.+x` that handles both file-save and directory-snapshotting.
- Proves that the "Muscle" doesn't care about the UI context.

---
**Vision:** By the end of this roadmap, the `file-op` system will be a "Black Box" that any TPMOS project can plug into. This proves that the OS is not a collection of apps, but a collection of **Sovereign Handshakes**.
