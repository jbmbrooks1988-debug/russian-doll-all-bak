# TPMOS Architecture Note: The "Global Projection" Problem
**Date:** 2026-05-23
**Subject:** Manager State Collision in Global Layouts



## 8. Final Known Bugs & Next Task (Continued)

3. **Interact View Auto-Render Failure**:
   - **Symptom**: The interact window (editor view) does not render automatically upon loading/switching; it requires a manual "down arrow" navigation to force a refresh.
   - **Root Cause**: The Parser's `compose_frame` loop is not receiving the render pulse immediately after the layout switch. This is a synchronization issue between the layout transition and the `frame_changed.txt` trigger.
   - **Task**: In `agy-text-editor_manager.c`, ensure `trigger_render()` is called immediately after `transition_to_layout()`.

4. **File Op Silent Failure (Absolute Paths)**:
   - **Symptom**: File operations fail to write.
   - **Root Cause**: `file_op.+x` receives the path as a partial string in some contexts. The shell execution requires absolute paths to be safe.
   - **Task**: Update all `system()` calls in `agy-text-editor_manager.c` to construct and verify absolute paths *before* building the shell string.

---
*"The plumbing is global, the marker is the clock, but the trigger is the heartbeat. Flush, marker, pulse. Do not miss the pulse."*
