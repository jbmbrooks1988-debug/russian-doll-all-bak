# ⚠️ Chapter 16: Pitfalls & Debugging the Ops Pipeline
In a decentralized, file-based OS, bugs manifest differently than in traditional monolithic apps. This chapter covers the most common "Traps" in v6.00. 🪤🐛

---

## 🚫 The "Zombie Registry" Trap
**Symptom:** You uninstalled a project via Fondu, but its buttons still appear in the OS.
**Cause:** The `ops_catalog.txt` was not correctly flushed, or the `chtpm_parser` is still using a cached version of the layout.
**Fix:** 
1.  Verify the project is removed from `pieces/apps/installed/`.
2.  Run `fondu --list-ops` to force a catalog refresh.
3.  Delete `pieces/display/frame_changed.txt` to force a hard re-render.

---

## 🚫 The "Stale State" Race Condition
**Symptom:** Two Ops update the same `state.txt` simultaneously, and one update is lost.
**Cause:** Failing to use the **Atomic Swap Pattern**.
**Fix:** Always write to `state.txt.tmp` first, then use `rename("state.txt.tmp", "state.txt")`. The OS filesystem ensures that `rename()` is an atomic operation.

---

## 🚫 The "Broken Variable" Null
**Symptom:** Your UI shows `${player_hp}` instead of the actual number.
**Cause:** 
*   The variable name in `.chtpm` doesn't match the key in `state.txt`.
*   The `chtpm_parser` cannot find the piece directory.
**Fix:** Check `piece.pdl` for the correct `piece_id` and ensure the directory structure matches Chapter 2.

---

## 🛠️ The Master Ledger Debugger
If you are lost, follow the breadcrumbs.
`tail -f pieces/os/master_ledger.txt`
This will show you every Op call and every state change in real-time. If the ledger is silent, your Module (Brain) is likely dead.

---

## 💻 Code Example: Defensive State Reading
```c
char* safe_read_state(const char* path, const char* key) {
    int retries = 3;
    while (retries--) {
        char* val = get_state_value(path, key);
        if (val) return val;
        usleep(10000); // Wait for file lock to release
    }
    return "N/A";
}
```

---

## 🏛️ Scholar's Corner: The "Ghost Click"
In v5.9, a developer reported that buttons were being "clicked" by ghosts. It turned out their `history.txt` wasn't being cleared after a command was read. The Brain would read `MOVE:up`, fork the Op, and then on the next loop, read the same line again! In v6.00, the **Standardized Brain Template** mandates that the input file MUST be truncated to zero bytes immediately after a command is successfully parsed. 👻🖱️

---

## 📝 Study Questions
1.  Why is `rename()` preferred over direct file writing for state updates?
2.  What should you do if a CHTPM variable fails to substitute?
3.  How does the `master_ledger.txt` assist in debugging?
4.  **Scenario:** Your Op is running, but the UI isn't updating. What is the most likely cause?

---
[Return to Index](INDEX.md)
