# Wraith 3D Cube

This is a nested Wraith project.

Purpose:
- Prove that a Wraith-owned 3D probe project lives under `projects/wraith-alpha/wraith-projects/`.
- Keep it discoverable by the Wraith launcher/terminal flow, not the global top-level project loader.
- Use it later to validate `${game_map}`, `INTERACT`, `is_map_control`, and RGB/ASCII parity from inside the Wraith session.

Notes:
- This now includes the standards-compliant z-slice piece source:
  - `pieces/cube_probe/artifact.txt`
  - `pieces/cube_probe/state.txt`
- The next priority is RGB conversion of that piece source inside the `${game_map}` surface.
- See `IMPLEMENTATION.md` for the semantic export and converter plan.
