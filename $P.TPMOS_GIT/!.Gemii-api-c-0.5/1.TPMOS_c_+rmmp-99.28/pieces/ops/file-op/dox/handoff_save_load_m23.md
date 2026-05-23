# TPMOS Handoff: Sovereign Save/Load Handshake & CLI Testing Protocol
**Date:** 2026-05-23
**Status:** CLI VERIFIED / PRODUCTION PENDING (Race Condition Identified)

## 1. Architectural Resolution: The PDL Handshake

We have resolved the "Isolated Memory" problem by establishing the **Project PDL** (`project.pdl`) as the sovereign source of truth.

### The Handshake Mechanism:
1.  **Muscle (The Op)**: `file_manager.+x` performs the disk operation (Save/Load).
2.  **Handshake (The Write)**: Upon success, `file_manager` calls `sync_pdl_op.+x` to update the `STATE | active_file` field in `project.pdl`.
3.  **The Pulse (The Trigger)**: `file_manager` appends to `projects/<id>/manager/state_changed.txt`.
4.  **Reaction (The Sync)**: `agy-text-editor_manager.+x` polls for that pulse. When detected, it re-reads the PDL and reloads the document buffer.

## 2. Verified Proofs (CLI Verification)

Both managers support direct CLI verification. **These tests prove the logic is correct in isolation.**

### Test A: Simulate File Browser Save/Load
```bash
cd 1.TPMOS_c_+rmmp-99.29
./pieces/ops/file-op/+x/file_manager.+x --test-save agy-text-editor projects/agy-text-editor/proof.txt
```
**Result**: `Success: Saved and Handshaked.` (Confirmed via `cat projects/agy-text-editor/project.pdl`)

### Test B: Verify Editor Synchronization
```bash
cd 1.TPMOS_c_+rmmp-99.29
./projects/agy-text-editor/manager/+x/agy-text-editor_manager.+x --test-load-pdl
```
**Result**: `Current Active File from PDL: projects/agy-text-editor/proof.txt`

## 3. The "Production Gap" (Next Task for New Agent)

While the CLI logic works, it may fail in the CHTPM Framework/Nav due to an **Asynchronous Handover Race**.

### Problem Analysis:
- In `file_manager.c`, the call to `transition_to_layout(return_layout)` is immediate.
- The CHTPM Parser may react to this layout change by terminating the `file_manager` process **before** the `run_command(handshake)` or `trigger_state_pulse()` have finished their filesystem I/O.
- If the pulse is never hit, the Editor manager wakes up but never re-reads the PDL, resulting in a stale "Active" variable.

### Mandatory Fix for Production:
1.  **Sync-Before-Swap**: Ensure the `file_manager` waits for the `sync_pdl_op` to return an exit code 0 before calling `transition_to_layout`.
2.  **The Double-Marker Handshake (Acknowledge Pattern)**:
    - To eliminate the race condition, implement an **Acknowledgment Marker**.
    - **Step 1 (Browser)**: Writes to PDL and appends to `state_changed.txt` (The Request).
    - **Step 2 (Browser)**: Polls `sync_confirmed.txt` in a loop (with timeout).
    - **Step 3 (Editor)**: Sees the pulse, syncs internal state, and appends to `sync_confirmed.txt` (The Ack).
    - **Step 4 (Browser)**: Sees the Ack and *then* calls `transition_to_layout`.
    - This ensures the process is never killed until the Editor has "handshaked" back.

3.  **Marker Atomicity**: The pulse in `state_changed.txt` must be the absolute final act before the process relinquishes control.
3.  **Path Resolution Audit**: Verify that `current_dir` and `project_id` in the UI context correctly map to the absolute paths expected by the Ops.

## 4. Engineering Improvements Applied
- **Mode-Based UI**: Separated "Menu" from "Editing" to stop digit-conflict input bugs.
- **Informative Headers**: Editor now displays the active filename in the header.
- **Thin Manager Compliance**: All filesystem muscle delegated to Ops.
- **No Residual Apps**: `player_app` dependency removed; handshake is project-sovereign.
