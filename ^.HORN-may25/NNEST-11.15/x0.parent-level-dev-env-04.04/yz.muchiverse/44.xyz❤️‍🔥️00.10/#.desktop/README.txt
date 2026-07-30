#.desktop — House "desktop tray" for portable pieces
====================================================
House: 44.xyz…  |  Date: 2026-07-28

WHAT
----
A **file desktop** (not the OS wallpaper): a shared folder where pickers,
event-editor, zoo/pets, and mutaclysm exchange **portable piece packages**.

Mental model:
  tile-picker / map assets  → place onto #.desktop/
  event-editor              → open/edit packages on #.desktop/ (or muta live)
  pets / charas             → can live as desktop windows + folders here
  mutaclysm                 → DROP / import from #.desktop/ into world_01

This is the same idea as `exchange/` for pet envelopes, but **maker-wide**:
events, tiles, entities, not only pets.

LAYOUT
------
  #.desktop/
    events/      # event packages (event.ir + event.pal + state)
    entities/    # pets, NPCs, charas (piece.pdl + state + optional events)
    tiles/       # tile / emoji brush stamps (glyph + meta)
    inbox/       # optional drop zone watched by focused mutaclysm
    README.txt   # this file

Override root: env XYZ_DESKTOP_ROOT=/abs/path

RULES
-----
  - Packages are directories (or single .pdl + sidecar), never opaque blobs only.
  - SAVE/LOAD of mutaclysm copies world_01; desktop is **outside** the live
    world until imported (so you can edit offline, then drop in).
  - Drag-drop (X11) can target desktop windows later; file ops are source of truth.

See: 101.mutaclsym…/dox/xelector-context.md § desktop + event-editor widget
     &.widgits/event-editor/
