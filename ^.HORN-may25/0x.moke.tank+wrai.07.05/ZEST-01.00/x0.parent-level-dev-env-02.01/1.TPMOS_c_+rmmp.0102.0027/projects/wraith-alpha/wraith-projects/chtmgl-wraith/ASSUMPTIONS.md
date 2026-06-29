# CHTMGL Wraith Assumptions

Reference:
- `projects/chtmgl-alpha/layouts/index.chtmgl`

Assumptions being tested:
- CHTMGL informs Wraith's rich UI primitive vocabulary: `panel`, `button`, `checkbox`, `slider`, `menu`, media/image placeholders, and canvas-like preview surfaces.
- Wraith should keep runtime payload naming as `${game_map}` for now, even when the source reference says `canvas`.
- map/canvas controls can live in side panels, headerbars, menus, footers, or overlays if scene records declare `target_surface=game_map`.
- normal Wraith window chrome remains separate from project-local controls.

Current limits:
- static scene records exist, but no CHTMGL parser exists yet.
- RGB presenter has first-pass widget shapes, not full CHTMGL style/layout fidelity.

Next correction target:
- write a project-owned parser/manager that reads `layouts/chtmgl-wraith.chtpm` or a copied CHTMGL file and emits `session/scene.objects.pdl`.
