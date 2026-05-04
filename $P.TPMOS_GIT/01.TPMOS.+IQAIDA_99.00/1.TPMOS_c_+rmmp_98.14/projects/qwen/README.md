# Qwen Project
ASCII interface for local Qwen LLM integration.

## Features
- Local CLI interface via CHTPM.
- AI Bridge to `qwen` system command.
- `iqabel` agent piece with FSM/RL capabilities.
- Automated PAL script generation (Planned).

## Usage
Run `./run.sh` from this directory.

## Structure
- `layouts/`: CHTPM layout files.
- `manager/`: C-based project orchestrator.
- `ops/`: AI bridge and other operational binaries.
- `pieces/`: Contains the `iqabel` agent piece.
