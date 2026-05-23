# TPMOS Architectural Note: Save/Load Pipeline & Module Handover
**Date:** 2026-05-23
**Subject:** Resolving State Desync between Editor and Global File Browser

## 1. The Current Logical Pipeline (Discovery)

The failure to update the `active` file variable stems from a **Module Handover** issue.

1.  **Editor State**: `agy-text-editor_manager` owns the current session, holding `active_file_path` in memory and writing it to `projects/agy-text-editor/manager/gui_state.txt`.
2.  **Transition**: When "Save As" is clicked, the Editor calls `transition_to_layout("pieces/ops/file-op/layouts/file_browser.chtpm")`.
3.  **Module Switch**: The CHTPM Parser stops the Editor's projection and launches `pieces/ops/file-op/+x/file_manager.+x`.
4.  **Sovereignty Loss**: `file_manager` now owns the projection. It reads from its own `gui_state.txt`. It **does not know** about the Editor's memory or its specific `active_file_path` variable.
5.  **The Save Action**: `file_manager` performs the `file_op save`. It writes "Saved File: ..." to the response line.
6.  **The Desync**: Because `file_manager` is a generic tool, it doesn't know it should update `agy-text-editor`'s `project.pdl`. It only updates the file on disk.
7.  **Return**: When the user returns to the Editor, the Editor's manager (which might have been restarted or stayed resident) is still using its old `active_file_path`.

## 2. The Solution: Shared Source of Truth

To ensure "Save and Load" always work, we must move the "Active File" from Manager memory to a **Sovereign State File**.

### Step 1: Shared Project State
Both the Editor and the File Manager must use `pieces/apps/player_app/manager/state.txt` as the transient source of truth for the *current project context*.

### Step 2: The "Thin" File Manager (Op Delegation)
The `file_manager` module should be thinned out. Its ONLY job is to:
- List directories (via `list_dir_op`).
- Capture the user's desired path.
- Call `file_op save` or `load_file_op`.
- **CRITICAL**: Upon success, it must update the `active_project` key in `pieces/apps/player_app/manager/state.txt`.

### Step 3: Editor Pulse (Detection)
The `agy-text-editor_manager` must poll `pieces/apps/player_app/manager/state.txt` for changes to `active_project`.
- If `active_project` changes while the Editor is active, it reloads the buffer from that new path.
- This ensures that if a file is loaded via the global browser, the editor immediately reflects the new content.

## 3. Why the Next Edits Will "Definitely Work"

- **No Memory Assumption**: By using `player_app/manager/state.txt`, we bridge the gap between two different binaries (`agy-text-editor` and `file_manager`).
- **Absolute Paths**: All components will resolve the path relative to the absolute `project_root` (from `location_kvp`), eliminating "file not found" errors during nested navigation.
- **Marker Synchronization**: We will use the `state_changed.txt` marker to force the Editor to re-read the state file only when an Op has actually moved the needle.

## 4. Logical Flow of the Fix:

1.  **User types `hi-3.txt`** -> `file_manager` sees it.
2.  **User clicks "SAVE FILE"** -> `file_manager` calls `file_op save projects/agy-text-editor/pieces/document.txt projects/agy-text-editor/hi-3.txt`.
3.  **Update Truth** -> `file_manager` calls `sync_op` to set `active_project=projects/agy-text-editor/hi-3.txt` in `player_app/manager/state.txt`.
4.  **Pulse** -> `file_manager` hits `state_changed.txt`.
5.  **Editor Sync** -> `agy-text-editor` sees the pulse, reads the new `active_project`, updates its `active_file_path`, and refreshes the UI.

This creates a **State-Driven Handover** instead of a **Command-Driven Handover**, which is the only way to achieve multi-module compatibility in TPMOS.
