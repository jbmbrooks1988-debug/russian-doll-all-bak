# TPMOS Logical Pipeline: Sovereign Save/Load Handover
**Date:** 2026-05-23
**Subject:** Ensuring Variable Update Consistency Across Module Boundaries

## 1. The Core Problem: Memory Isolation
The `agy-text-editor` and `file_manager` are two separate binaries. When you switch to the "Save As" screen, the Editor's memory (which holds the `active_file` variable) becomes inaccessible. The File Manager saves the file to disk but fails to "tell" the Editor where it put it. When you return to the Editor, it still thinks it's working on the old file.

## 2. The Solution: The "Sovereign PDL Handshake"
We will implement a robust handshake using the project's own `project.pdl` file.

### Phase A: The Theater (CHTPM Parser)
- The Parser maintains the `<cli_io>` buffers. 
- Shared streams (`history.txt`, `cli_buffers.txt`) live in `pieces/os/`.

### Phase B: The Browser (File Op Manager) - THE ACTION
1.  **Input Capture**: User types `hi-3.txt`. `file_manager` reads this from `pieces/os/cli_buffers.txt`.
2.  **Thin Muscle (Op Delegation)**:
    - Calls `list_dir_op.+x` for visual listing.
    - Calls `file_op.+x save` for the disk operation.
3.  **Sovereign Handshake**:
    - **CRITICAL**: If the Save Op succeeds, `file_manager` calls `sync_op.+x` to update `projects/agy-text-editor/project.pdl` with `STATE | active_file | projects/agy-text-editor/hi-3.txt`.
    - It then hits `projects/agy-text-editor/manager/state_changed.txt`.
4.  **Handover**: Calls `transition_to_layout` to return.

### Phase C: The Editor (Agy Manager) - THE REACTION
1.  **The Watcher**: In its loop, it polls `manager/state_changed.txt`.
2.  **Variable Sync**:
    - Re-reads `project.pdl`.
    - Syncs its internal `active_file_path`.
    - Reloads the document buffer.
3.  **Visual Update**: Proactively triggers render.

## 3. Why this "Definitely Works"
- **Project Sovereignty**: The PDL is the project's DNA. It survives binary restarts.
- **No Residual Apps**: All state is either core OS or Project-owned.
- **Pure Ops**: Zero hardcoded filesystem logic in the managers.

## 4. Execution Roadmap:
1.  **Refactor `file_manager.c`**: Thin out directory listing and ensure it writes to shared `state.txt` upon successful save/load.
2.  **Refactor `agy-text-editor_manager.c`**: Add the `state_changed.txt` pulse-listener to sync its internal `active_file_path` with the OS state.
3.  **Compile & Verify**: Ensure both binaries build and the "active" variable updates immediately in the UI.
