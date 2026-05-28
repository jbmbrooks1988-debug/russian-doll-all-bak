# MISSION: FIX GEM-DEV TOOL INTEGRATION (GEMINI API)

## 1. THE PROBLEM
The `gem-dev` project (and its twin `gem-api`) is failing to execute tool calls (function calling) returned by the Gemini API. While the LLM is attempting to call tools like `read_file` or `exec_cmd`, the manager isn't successfully executing them and feeding the results back.

## 2. THE STACK
- **Manager:** `projects/gem-dev/manager/gem-dev_manager.c` (Orchestrator)
- **Parser:** `projects/gem-dev/ops/src/json_parser.c` (Surgical Dot-Notation Extractor)
- **Payload Builder:** `projects/gem-dev/ops/src/gemini_payload_builder.c` (Context -> Gemini JSON)
- **Ops:** `projects/gem-dev/ops/+x/` (Individual tool binaries)

## 3. KEY PITFALL: THE "FINICKY" JSON PARSER
Our `json_parser.c` is NOT a full JSON library. It is a custom, surgical string-walker.
- **Dot-Notation:** It handles `a.b[0].c`, but it is extremely sensitive to whitespace and escaping.
- **Escape Traps:** If the LLM returns a `functionCall` inside a string (escaped), the parser might fail to unescape the nested JSON properly before trying to extract keys like `name` or `args`.
- **The "ACTING" State:** The manager must transition to `ACTING`, run the binary, capture output, and then **APPEND** the result to `context.json` using the `tool` role.

## 4. THE EXECUTION PIPELINE (WHERE TO LOOK)
1. **Detection:** `check_ai_status()` uses `json_parser` to look for `candidates[0].content.parts[0].functionCall`.
2. **Extraction:** If found, it extracts `name` and `args`. **CRITICAL:** Check if `args` is being extracted as a raw JSON string or if it needs further unescaping.
3. **Dispatch:** The manager uses a long `if/else if` block to map `tool_name` to binary calls (e.g., `list_dir`, `file_ops`).
4. **Feedback:** After execution, the `result` MUST be appended to `context.json` via `json_state append ... tool ...`.
5. **Re-Query:** The manager must then call `start_ai_query(NULL)` to send the tool result back to Gemini so it can finish its thought.

## 5. YOUR TASKS
1. **Trace the JSON:** Inspect `projects/gem-dev/state/llm_response.json` after a tool attempt. 
2. **Test the Parser:** Manually run `./projects/gem-dev/ops/+x/json_parser projects/gem-dev/state/llm_response.json "candidates[0].content.parts[0].functionCall"` and see what it spits out.
3. **Fix the Manager:** Ensure the dispatcher in `gem-dev_manager.c` correctly handles the `args` JSON.
4. **Validate the Loop:** The goal is to ask "List files in the sandbox" and have the agent successfully run `list_dir` and show you the result.

## 6. MANDATE
- **HANDS OFF GEM-API:** Under NO circumstances are you to touch, modify, or "upgrade" anything in `projects/gem-api/` unless the user gives you explicit, line-by-line permission. `gem-api` is the stable baseline; treat it as read-only. All work happens in `projects/gem-dev/`.
- Do NOT replace the surgical parser with a heavy library like cJSON.
- Respect the **TPM Hierarchy**: Manager orchestrates, Ops execute.
- Always check `projects/gem-dev/gem-dev-log.txt` for recent pitfalls.

**"Geography is destiny. If it's not in a file, it's a lie."**
