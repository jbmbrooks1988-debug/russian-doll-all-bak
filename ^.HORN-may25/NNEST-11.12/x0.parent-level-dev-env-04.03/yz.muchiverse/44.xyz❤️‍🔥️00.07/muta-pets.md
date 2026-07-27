# MUTA-PETS: Pet Import/Export Architecture Analysis

**Created**: 2026-07-26  
**Status**: Corrected after discovering muchi-pals already has per-pet GL windows

---

## Key Discovery: muchi-pals Already Has Per-Pet Windows

**User was right**: muchi-pals already has individual GL windows for each pet via `system/egg_window.c`.

### What muchi-pals Has:

1. **Per-pet directories**: `pieces/world_01/map_lobby/egg_1/`, `egg_2/`, etc.
   - Each pet has its own directory with full state
   - `state.txt` - hunger, energy, position, species, etc.
   - `piece.pdl` - Piece definition with methods
   - `sprite.csv` - Visual sprite data
   - `atlas.png` - Sprite texture
   - `window.pid` - egg_window process ID

2. **Individual GL windows**: `system/egg_window.c`
   - Creates shaped GL window per pet (X11 Shape Extension + GLX)
   - Pet sprite rendered with alpha silhouette clipping
   - Moves on invisible desktop grid
   - Can be dragged with mouse (snaps to grid on release)
   - Self-ticks its own pet (independent of terminal)
   - Outlives terminal session (real desktop pet behavior)

3. **"Open Window" menu option**: Spawns egg_window for selected pet

### What's Missing:

1. **pet_export.c** - The op that moves pet directory to exchange
2. **pet_import.c** - The op that reads from exchange and creates native pet
3. **Exchange directory connection** - muchi-pals doesn't reference the exchange

---

## The Exchange Mechanism

### How It Works (from zoo project):

**Export flow** (pet_export.c):
1. Find pet directory in `pieces/world_01/map_<map>/<pet_id>/`
2. Write `trade_envelope.txt` into pet directory (neutral format)
3. Physically `rename()` directory to `exchange/<pet_id>/`
4. Pet is GONE from source game

**Import flow** (pet_import.c):
1. Find pet directory in `exchange/<pet_id>/`
2. Read `trade_envelope.txt` (origin_game, stats, etc.)
3. Translate stats to destination game's schema
4. Write fresh `state.txt` + `piece.pdl` in destination's world
5. Physically `rename()` directory into destination's pieces/
6. Pet is now fully native in destination game

### Exchange Directory Location:

**Current location** (from older project tree):
```
/home/no/Desktop/.../z0.egg-pals+plats-8.00/exchange/
```

**Status**: Exists but empty (no pets have been exported yet)

---

## Why It Won't Work Immediately

### Problem 1: muchi-pals Has No Export Op

muchi-pals has:
- `export_card.c` - Exports pet stats as PNG trading card image
- NO `pet_export.c` - The directory-move-to-exchange op

**muchi-pals's `export_card.c`** exports a visual card, NOT a directory to exchange.

### Problem 2: Exchange Directory Not Wired

muchi-pals's `button.sh` doesn't set `PRISC_EXCHANGE_ROOT`. The exchange directory is in an older project tree, not connected to the current muchi-pals instance.

### Problem 3: Destination Game Needs Import Op

Even if we export from muchi-pals, the destination (mutaclsym) needs `pet_import.c` to:
- Read the `trade_envelope.txt`
- Translate species/stats to mutaclsym's schema
- Create native pet in mutaclsym's world

---

## What Would Make It Work

### Step 1: Add pet_export to muchi-pals

**Copy from zoo**: `ops/pet_export.c` → `01.muchi-pals-🥚️-13.01/ops/`

**Adapt**:
- Change path resolution to find pets in `pieces/world_01/map_lobby/`
- Ensure `PRISC_EXCHANGE_ROOT` is set in button.sh
- Test: Export a pet → verify it appears in exchange/

### Step 2: Add pet_import to mutaclsym

**Copy from zoo**: `ops/pet_import.c` → `101.mutaclsym🧟‍♂️️+18.00/ops/`

**Adapt**:
- Change destination path to mutaclsym's world structure
- Add species translation (muchi-pals species → mutaclsym monsters)
- Test: Import pet from exchange → verify it appears in mutaclsym

### Step 3: Wire the Exchange

**Create shared exchange directory**:
```bash
mkdir -p /home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST-11.12/x0.parent-level-dev-env-04.03/yz.muchiverse/44.xyz❤️‍🔥️00.07/exchange
```

**Set in both button.sh files**:
```bash
export PRISC_EXCHANGE_ROOT="/home/no/Desktop/.../exchange"
```

### Step 4: Test Round-Trip

1. Launch muchi-pals, open pet window
2. Export pet to exchange
3. Launch mutaclsym, import pet
4. Verify pet appears in mutaclsym's world
5. Export pet back to exchange
6. Re-import to muchi-pals
7. Verify pet state preserved

---

## The Visual Layer (What's Already Built)

### muchi-pals egg_window.c:

**Features**:
- Shaped GL window (X11 Shape Extension + GLX)
- Pet sprite with alpha silhouette clipping
- Grid-based movement (invisible desktop grid)
- Mouse drag with grid-snap on release
- Self-ticking (independent of terminal)
- Process lifecycle logging to master_ledger.txt

**This is exactly what we need for desktop pets!**

### What's Missing for True Drag-Drop:

1. **Xdnd protocol** - X11 drag-drop between windows
2. **Window-to-window communication** - Drop event triggers import
3. **Automatic window closing** - Pet window closes when imported
4. **Automatic window spawning** - Pet window opens when exported

---

## Recommended Path Forward

### Phase 1: File-Level Working (1-2 hours)

1. **Create shared exchange directory**
2. **Copy pet_export.c to muchi-pals**, adapt it
3. **Copy pet_import.c to mutaclsym**, adapt it
4. **Set PRISC_EXCHANGE_ROOT in both button.sh files**
5. **Test**: Export from muchi-pals → Import to mutaclsym

### Phase 2: Visual Integration (2-3 hours)

1. **Add drop handler to mutaclsym** - When pet window dropped on mutaclsym window, trigger import
2. **Add close-on-import to muchi-pals** - When pet exported, close its egg_window
3. **Add spawn-on-export to mutaclsym** - When pet exported, spawn new egg_window on desktop

### Phase 3: Polish (1-2 hours)

1. **Smooth animations** - Window movement during transfer
2. **Species translation** - muchi-pals species → mutaclsym monsters
3. **State preservation** - Ensure all stats transfer correctly

---

## Summary

**muchi-pals already has the hard part**: per-pet GL windows that move on desktop.

**What's missing**:
- `pet_export.c` in muchi-pals (directory move to exchange)
- `pet_import.c` in mutaclsym (directory move from exchange)
- Exchange directory wired between both games

**This is doable in 1-2 hours for file-level, 3-4 hours for visual integration.**

---

## See Also

- `01.muchi-pals.../system/egg_window.c` - Per-pet GL window implementation
- `002.zoo.../ops/pet_export.c` - Export implementation to adapt
- `002.zoo.../ops/pet_import.c` - Import implementation to adapt
- `002.zoo.../dox/pet-import-export-standard.md` - Original design doc
