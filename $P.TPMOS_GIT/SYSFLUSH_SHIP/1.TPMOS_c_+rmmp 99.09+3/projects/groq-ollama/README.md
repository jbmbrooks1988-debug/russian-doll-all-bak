# Groq-Ollama TPMOS Agent
**Status:** Canonical TPMOS Integration Active
**Model:** Dynamic Resolution (Ollama Tags)

## 1. Overview
This project is a high-fidelity integration of the **Aida** agent logic into the TPMOS framework. It replaces the legacy `qwen` project with a multi-turn, tool-calling agent capable of local system interaction.

## 2. Usage
- **Input:** Type your query into the "Ask Agent" `cli_io` area and hit **Enter**.
- **Thinking:** The agent will flip to `THINKING` state. You can monitor progress in the `AI STATE` field.
- **Tools:** If the agent decides to use a tool (read/write files, list dirs), it will transition to `ACTING` and the `FSM` field will show the active tool.
- **Response:** Final responses are formatted with ASCII borders and rendered in a scrollable region.

## 3. UI Controls
- **[1] Clear Context:** Wipes `state/context.json` and resets the conversation.
- **[2] Switch API:** Toggles between endpoints defined in `config/apis.txt`.
- **[3] Summarize:** Triggers a conversation condensation cycle.
- **[ESC] Exit:** Returns to the TPMOS Project Loader.

## 4. Known Bug: "The Single File" / Directory Listing
**Symptom:** When asking the agent to `list_dir`, it occasionally only reports a single file (e.g., `test-file.txt`) instead of the full directory.

**Investigation:**
- The standalone `Aida` agent correctly lists its working directory.
- In TPMOS, the manager resolves the `project_root` via `location_kvp` but may not be performing a strict `chdir()`.
- If the manager process is launched from a specific context (like the loader), relative paths like `./` might resolve to unexpected locations or be filtered by the OS layer.

**Fix In Progress:**
We are refactoring the manager to perform an explicit `chdir(project_root)` at startup to align the process's working directory with the TPMOS Canonical Root.

## 5. Engineering Standards
- **One Writer Rule:** All UI variables are re-asserted from global state.
- **Marker Discipline:** Renders are only triggered on state changes.
- **Piece Audit:** All conversations are logged to `iqabel/memories/history.txt`.
