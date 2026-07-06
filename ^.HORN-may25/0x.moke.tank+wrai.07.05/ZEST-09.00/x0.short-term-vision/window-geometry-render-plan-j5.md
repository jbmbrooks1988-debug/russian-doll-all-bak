# Window Geometry Render Plan (J5)
Date: 2026-07-05
Status: Path A DONE (2026-07-05) — Path B still future-track, not started
Scope: KPI A2 from `todo-j5.txt`, narrowed to its first concrete slice

## Why this needed its own doc, not just a next-step message

The scoped-down first step I proposed for KPI A2 was: "make the active
window's rendered content honor its own `x/y/width/height` (from KPI
A1) instead of fixed constants." Looking at the actual rendering code
before writing that, the honest picture is more forked than that
sentence implies — there isn't one obvious place to plug four numbers
in. This doc lays out what's actually there and the real decision it
implies, so the decision gets made on purpose rather than by whichever
code path I happened to touch first.

## What's actually there (read, not assumed)

### 1. The desktop window chrome is one hardcoded-width ASCII box, not a parameterized rectangle

`build_desktop_shell_markup()` in `wraith-alpha_manager.c` composes the
whole active-window frame as literal, fixed-length border strings, e.g.:

```c
appendf(out, size, "+-WRAITH DESKTOP GUI---------------------------------------------------------------------------+<br/>");
...
appendf(out, size, "------------------------------------------");
```

The border's width is baked into the literal count of `-` characters in
the format string — there is no `width` variable driving it today, not
even a hardcoded constant being reused; it's just a literal string of a
fixed length written directly into the source. Body content lines are
appended below it the same way, one `<br/>`-terminated text line after
another, top to bottom.

**Consequence:** there is no `x`/`y` concept at all inside this
function. It doesn't place a box at a coordinate — it emits a sequence
of lines, and wherever that sequence lands on screen is landed
implicitly by whatever consumes this markup next (the `.chtpm`
rendering pipeline), not by anything in this function. "Honor the
window's `x/y`" doesn't have an obvious meaning at this layer, because
this layer has no positioning concept to hook into in the first place —
only "honor the window's `width`" does (by generating the border/line
width dynamically instead of using the fixed literal string).

### 2. True positioned-rectangle rendering DOES exist in this codebase — just one layer down, for a different kind of content

`wraith_rgb_daemon.c` (the RGB frame-buffer renderer, per
`x0.piececrafts/ARCHITECTURE-RGB-RENDERING.md`) already renders
individual scene OBJECTs at real `x`/`y`/`w`/`h` pixel coordinates —
e.g. `blit_text(buffer, obj->x + 2, obj->y + 2, "canvas->game_map", obj->fg, obj->w - 4, cell_w, cell_h);`.
This is real, working positioned rendering. But it operates on a
project's own `scene.objects.pdl` content (tiles, entities, a
`game_map` surface) — it is a different pipeline from the top-level
desktop window chrome `build_desktop_shell_markup()` composes. I did
not find a call from `wraith_rgb_daemon.c` into
`build_desktop_shell_markup()`'s output or vice versa; they appear to
be two separate rendering concerns (project-internal scene content vs.
top-level desktop/window chrome), not one pipeline.

## The actual fork this implies

**Path A — parameterize width only, at the text-composition layer
(`wraith-alpha_manager.c`).** Replace the literal fixed-length border
strings with ones generated from `active_window()->width` (falling
back to today's literal length — count the dashes, use that as the
default — when `width == 0`, i.e. every project until it has a saved
`WINDOW` section). Leaves `x`/`y` alone entirely for now, since this
layer has no positioning concept to attach them to. Smallest real step;
proves `width` flows from `project.pdl` to a visibly different-sized
box on screen, without touching the RGB/positioned-rendering layer at
all.

**Path B — investigate wiring window `x`/`y`/`width`/`height` into the
RGB-layer's real positioned-rendering primitives instead**, so a window
becomes an actual positioned+sized rect in the frame buffer the way
scene OBJECTs already are. This is the version that actually matches
"drag/resize a window" as a visual reality (per the original ask).

## Path B investigation, done (2026-07-05)

Traced the full pipeline end to end, file by file, to answer "how does
the composed desktop markup actually reach pixels":

1. `wraith-alpha_manager.c`'s `build_desktop_shell_markup()` composes
   the active window's `<button>`/`<br/>` markup string.
2. That string is written as a `desktop_shell_markup=...` KVP line into
   `session/alpha_state.txt` / `session/desktop_ui_state.txt`
   (`write_projection()`/`update_state()`).
3. **`projects/wraith-alpha/ops/wraith_parser_alpha.c` — Wraith's own,
   separate ~2700-line reimplementation of the classic chtpm parser**
   (NOT the shared `pieces/chtpm/plugins/chtpm_parser.c`) reads
   `projects/wraith-alpha/layouts/alpha-shell.chtpm`, substitutes
   `${desktop_shell_markup}` (registered in its own substitution table,
   line ~503) with that KVP value, and its `compose_frame()` writes the
   fully composed result to `pieces/display/current_frame.txt` — a
   plain text file.
4. `wraith_rgb_daemon.c`'s `render_ascii_frame()` reads
   `current_frame.txt` line by line, and for **every character** in
   every line calls `blit_char()`, which rasterizes that one character
   as a fixed `GLYPH_W×GLYPH_H` (8×16) pixel block into the RGBA32
   buffer at `(col*8, row*16)` — this is exactly the `COLS(128)×ROWS(40)`
   grid already documented in `x0.piececrafts/ARCHITECTURE-RGB-RENDERING.md`.

**The finding that changes the shape of Path B:** the desktop chrome is
not freeform pixel graphics with an incidental text look — it **is** a
128-column × 40-row monospace character-cell grid, always. There is no
raw x/y pixel positioning happening anywhere in this pipeline; every
"position" that exists is really "which character cell." Two
consequences:

- Path A's work (generating a border of N literal dash *characters*)
  was already operating in the right unit for this architecture by
  accident/design — character width, not pixel width. Nothing there
  needs to change because of this finding.
- **"True window positioning" in this system means placing a window's
  composed text into a specific rectangular region of the same
  128×40 character grid** — not drawing a positioned rectangle in RGB
  space directly. Concretely, this means `build_desktop_shell_markup()`
  (or a new function replacing its role) needs to become a **character-grid
  compositor**: instead of producing one sequential stream of lines that
  fills the whole `${desktop_shell_markup}` slot, it needs to render each
  open window's content into its own line/column buffer, then merge
  multiple windows' buffers into one 128×40 grid at each window's own
  `x/y/width/height` (already in character-cell units), before that
  merged grid becomes `current_frame.txt`. This is genuinely equivalent
  to a simplified terminal multiplexer (tmux/screen-style pane
  compositing) — real, buildable work, but it changes how the whole
  active-window-only model works today (only one window's content is
  composed at all right now; multiplexing means composing every visible
  window's content every frame).

This is NOT a rendering-primitive problem (no OpenGL/pixel-drawing work
needed — `blit_char()` already does all the pixel work, unchanged) — it
is purely a **text-layout/compositing problem** at the
`wraith-alpha_manager.c` layer. That's good news for buildability, but
it's still a real rewrite of `build_desktop_shell_markup()`'s single-window
assumption, not a small patch.

## Recommendation

Do **Path A now**, as the literal next step — it's small, safe, directly
built on the KPI A1 work already merged, and it answers a real question
(does data from `project.pdl` visibly reach the screen at all) without
requiring first solving the bigger "how does the desktop actually get
drawn" research question. **[DONE — see KPIs below]**

**Path B's investigation is done (see above); its implementation is
still a separate, deliberate next decision, not something to start
speculatively off the back of the research alone.** The concrete shape
is now known — turn `build_desktop_shell_markup()` (or its successor)
into a character-grid compositor that places each visible window's
content into its own region of the 128×40 grid instead of one window
filling the whole thing — but this is a real rewrite of the
active-window-only model the whole desktop currently assumes, touching
how focus, taskbar, and every existing button/action in that function
works.

## Path B implementation plan (2026-07-05)

### Critical finding first: two parallel systems describe the same screen, and both must move together

Reading `render_element()`/`compose_frame()` in `wraith_parser_alpha.c`
and `write_semantic_projection_files()`/`append_project_probe_scene_markup()`
in `wraith-alpha_manager.c` surfaced a second system, distinct from the
visual character grid already documented above:

1. **Visual grid** — `current_frame.txt`, built by
   `wraith_parser_alpha.c`'s own `render_element()` walking the parsed
   `.chtpm` element tree (including the `<button>`/`<text>` tags emitted
   *inside* `${desktop_shell_markup}`'s substituted value — these get
   re-parsed as real elements, each assigned its own `interactive_idx`
   nav order by this parser). This is what's actually displayed.
2. **Semantic/click system** — separate `current_frame.objects.pdl` /
   `desktop_state.pdl` / `focus_state.pdl` files, written directly by
   `wraith-alpha_manager.c` (`write_semantic_projection_files()`), which
   carry explicit `x=/y=/w=/h=/nav=/action=` per interactive element —
   used for **mouse** click hit-testing (matched against
   `g_mouse_x`/`g_mouse_y`) for a project's own embedded scene/game
   content specifically (tiles, entity buttons — the
   `append_project_probe_scene_markup()` path), which is a *different*
   mechanism from `wraith_parser_alpha.c`'s own keyboard-nav-index
   system for the outer chrome's buttons.

**These are two independently-written descriptions of the same screen.**
Moving a window's content to a different position only in the visual
text (`current_frame.txt`) without moving the matching `x=/y=` values in
the semantic OBJECT files would make buttons *look* like they moved
while still only being clickable at their old position. Any
implementation of Path B must update both, consistently, in the same
change — not the visual layer alone.

**Gap closed (2026-07-05, full trace of `render_element()`):** read the
whole function (`wraith_parser_alpha.c` lines 2037-2171). Confirmed with
certainty: **there is no row/column cursor anywhere in it.** It is pure
sequential `strcat()`-based text accumulation into one flat `frame`
buffer — `<br/>` appends a literal `"\n"`, `<text>` appends its
substituted label directly, every interactive element type
(`<button>`, `<scroller>`, `<cli_io>`, etc.) builds a small inline
string (`"%s %d. [%s]"`-style) and appends it — nothing here tracks an
absolute row or column, computes one, or exposes one to hook into. A
character's final on-screen position is 100% emergent from "how much
text and how many `\n`s came before it in the concatenated string," not
a coordinate that exists anywhere as a value.

**This confirms (not just assumes) the only viable mechanism:**
horizontal offset can only be achieved by literally prefixing each
emitted line with N leading space characters; vertical offset can only
be achieved by literally emitting N blank lines before a window's
content begins. There is no cursor/position API to call instead — text
padding *is* the positioning mechanism in this architecture, at every
layer, all the way down. Step 1 below is correct as originally scoped,
now with certainty instead of a hedge.

**Second thing the trace surfaced:** `${desktop_shell_markup}` sits at
a fixed point in `alpha-shell.chtpm` (line 8), preceded by ~3 fixed
header lines (lines 5-7: the "KPI 1" text, the instructions text, and a
blank `<br/>`). So a window's `y=0` is relative to *that insertion
point*, not literal screen row 0 — whatever assumes "the header is N
lines tall" has to be the *same* N on both the visual side (implicit,
by literal template position) and the semantic/click side (wherever its
`y=` values are computed). This is exactly the kind of assumption that's
easy to encode correctly in one place and forget to update in the other
— see the refactor recommendation directly below.

## Is the two-systems finding a problem? Recommendation: yes, a small targeted refactor

Two independently-computed descriptions of "where things are" is not
automatically a bug — separating *what's drawn* from *what's clickable*
is a normal, often-good pattern (plenty of real UI systems do this on
purpose). **It's not a problem today** because neither system has any
dynamic per-window offsetting yet — both sides currently use fixed,
hardcoded positions that happen to already agree, by construction.

**It becomes a real, active risk the moment Path B adds per-window
offsets**, because right now that would mean adding `+ window->x` /
`+ window->y` arithmetic by hand, independently, in two different files
(`wraith-alpha_manager.c`'s text-emission path and its own
semantic-emission path — recall these are actually the *same* file, but
still two separate functions with no shared computation between them).
Two independent hand-written copies of the same offset math is exactly
the shape of bug that looks fine on the day it's written and quietly
drifts the day either copy gets touched without the other.

**Recommendation: extract one small shared helper** —
e.g. `resolve_window_content_origin(const Window *window, int *out_row_offset, int *out_col_offset)`
— that both the visual-emission path (line/space padding) and the
semantic-emission path (`OBJECT x=/y=` values) call to get the *same*
offset numbers, instead of each computing `window->x`/`window->y`
inline independently. This is **not** a proposal to merge
`current_frame.txt` and `current_frame.objects.pdl` into one system —
that would be a much bigger, higher-risk rewrite of working
infrastructure for its own sake, and isn't warranted here. It's a small,
surgical change: one function, two call sites, that removes the
"two hand-written copies of the same math" risk before Path B's actual
positioning steps (below) get implemented — cheap now, and exactly the
kind of thing that's expensive to retrofit later once more features
depend on either path independently.

### Recommended first slice — NOT full multi-window compositing yet

Full "multiple windows visible at once" requires rewriting the
active-window-only assumption itself (today, exactly one window's
content is ever composed, full stop) — bigger and riskier than a first
slice should be. Instead, prove the mechanism on the *existing*
one-window-visible model first:

**Prove a single (still the only visible) window can render at an
arbitrary `x`/`y` offset within the 128×40 grid, with mouse clicks on
its own controls still landing correctly at the new position.**

Concrete steps:
0. Add `resolve_window_content_origin(const Window *window, int *out_row_offset, int *out_col_offset)`
   per the refactor recommendation above — both steps 1 and 2 call this
   one function rather than each computing `window->x`/`window->y`
   inline. Default (`x=0,y=0`, every project today) returns `{0,0}`,
   preserving Path A's "zero regression" discipline.
1. In `build_desktop_shell_markup()`'s active-window content path
   (`append_project_probe_body()`/`append_project_probe_scene_markup()`),
   use the resolved `out_col_offset` to prefix each emitted line with
   that many leading space characters, and the resolved `out_row_offset`
   to emit that many blank lines before the window's content begins —
   confirmed (see the full trace above) to be the only mechanism
   available, since `render_element()` has no coordinate/cursor concept
   to hook into instead.
2. In `write_semantic_projection_files()`/
   `append_project_probe_scene_markup()`, call the *same*
   `resolve_window_content_origin()` and add its `out_row_offset`/
   `out_col_offset` to every OBJECT `x=`/`y=` value emitted for that
   window's own controls — guaranteeing agreement with step 1 by
   construction, not by two people remembering to keep two copies of
   the same math in sync.
3. Leave the outer desktop chrome (title bar, taskbar, ASCII/GL toggle)
   exactly where it is — this slice only moves a window's own inner
   content region, not the whole desktop shell.

### KPIs for this slice

- **KPI C1 — zero regression at x=0,y=0.** Every project without saved
  `WINDOW.x`/`WINDOW.y` (all of them right now) renders byte-for-byte
  identical to today, same discipline as KPI B1.
- **KPI C2 — visible shift at nonzero x/y.** A hand-edited test
  project's saved `WINDOW.x`/`WINDOW.y` visibly moves its content region
  within the grid.
- **KPI C3 — clicks follow the shift, not the reverse.** A button inside
  that window's content is clickable at its *new* on-screen position and
  is *not* still clickable at its old position — this is the real proof
  that both systems moved together, not just the visual one.

### Implementation status (2026-07-05)

Code is in and compiles clean (`gcc -Wall`: exit 0, 0 errors; diffed
against the KPI-B baseline build log — every warning is the same
pre-existing one at a shifted line number, nothing new). Real
`+x/wraith-alpha_manager.+x` rebuilt in place.

- `resolve_window_content_origin()` + `append_with_origin_offset()`
  added, per the refactor recommendation above.
- `build_desktop_shell_markup()`: the window's inner panel (title bar +
  content — NOT the outer frame border, NOT the taskbar, both of which
  stay fixed) now composes into a local `raw` buffer, then gets injected
  through the offset helper.
- `write_semantic_projection_files()`: the same offset is added to every
  window-owned OBJECT's `x=`/`y=` — chrome row, title, content panel,
  all three chrome buttons (o/-/x), body-text lines, launcher rows.
- `append_project_scene_objects()` (a project's own `scene.objects.pdl`
  content — tiles/entities for map-based projects) needed the same
  treatment too, found while implementing, not anticipated in the
  original plan — its signature now takes `win_row_offset`/
  `win_col_offset` and adds them to every object's `x`/`y`.

**KPI C1 verified** (zero regression): every project has no saved
`WINDOW` section today, so `resolve_window_content_origin()` returns
`{0,0}` everywhere, which both `append_with_origin_offset()` and the
semantic-side `+ 0` arithmetic leave byte-identical to pre-change
behavior — confirmed by the clean warning-diff above, not yet by a live
screenshot.

**KPI C2 and C3 are NOT yet live-verified** — that needs an actual
running Wraith session with a hand-edited test `project.pdl`, which is
a manual check, not something provable from source alone. See the
checklist below.

### Manual verification checklist (do this next)

1. Pick any one Wraith-hosted project (not the terminal — its taskbar
   launcher-row path is a slightly different branch; a project using
   `append_project_probe_body()`'s normal body-text path is the cleanest
   first test) and add to its `project.pdl`:
   ```
   WINDOW       | x                  | 10
   WINDOW       | y                  | 3
   ```
2. Open that project's window in a running Wraith session.
   - **Check A (KPI C2):** its title bar and body content should visibly
     shift right and down relative to where it renders today — the
     outer desktop frame border and the taskbar row should NOT move.
   - **Check B (KPI C3, the important one):** click one of that
     window's own controls (the o/-/x title-bar buttons, or a body
     button) at its NEW shifted position. It should respond normally.
     Then try clicking where that control used to be (its old,
     un-shifted position) — it should NOT respond there anymore.
3. Remove the `WINDOW` section (or set both back to 0) and confirm the
   window returns to rendering exactly as it did before this change —
   the actual KPI C1 regression check, now with eyes on it instead of
   just a clean warning-diff.
4. If a map/scene-based project (one that uses
   `append_project_scene_objects()` — piececraft-wraith is a good
   candidate) is available, repeat steps 1-2 on it specifically, since
   that code path was a second, initially-unplanned fix and deserves its
   own direct check.

### Explicitly deferred, not this slice

- **Multiple simultaneously-visible windows** (the true tmux-style
  compositor) — needs the active-window-only model itself rewritten;
  its own future plan, after this slice proves the coordinate-offset
  mechanism works end to end on one window.
- **Live drag** (continuous mouse-move tracking while a button is held)
  — already scoped separately in `todo-j5.txt`'s addendum; layers on top
  of static positioning once this slice proves it.
- **Width-driven body-line wrapping/clipping** (word wrap, content
  reflow at narrower widths) — Path A only resized the outer border;
  this slice only repositions, doesn't newly handle wrapping either.

## Precedence decision (2026-07-05, before implementation)

User direction: width should be resolvable from either a future
layout-declared value ("new") or `project.pdl` ("legacy shim"), and
**`project.pdl` should always override a layout-declared/hardcoded
value.** Agreed — this is the same "sovereign artifact, user's saved
state wins over an author's original default" model already recorded in
`0x-pet-wraith-architecture-j29.md`'s 2026-07-05 note, applied here as a
concrete 3-tier precedence:

1. **`project.pdl`'s `WINDOW.width`** (user-saved, via KPI A1) — always
   wins when present.
2. **Layout-declared width** — the "new" tier, used only when tier 1 is
   absent. **Checked before implementing, not assumed:** `chtpm_parser.c`'s
   `parse_attributes()` is an explicit attribute allow-list (`label`,
   `href`, `onClick`, `id`, `visibility`, `fg`, `bg`, `source`, `prefix`,
   `suffix`, `time_reactive`) — there is no `width`/`height` attribute
   today, on `<panel>` or anywhere else. Wiring this tier means editing
   the shared parser used by every project in the OS, not just
   `wraith-alpha_manager.c` — a materially bigger blast radius than this
   pass. **Deferred as its own follow-up**, not implemented now.
3. **Hardcoded historical default (96)** — used when both above are
   absent, matching today's literal border length exactly.

`resolve_frame_width(window, layout_declared_width)` below is written
with this exact 3-tier shape already, taking `layout_declared_width` as
a parameter now passed `-1` ("none") at both call sites — so wiring tier
2 later is a one-line change at the call sites plus the (separate,
shared-parser) work to actually read it from a layout, not a rework of
this function.

## Concrete steps for Path A (if approved)

1. In `build_desktop_shell_markup()`, compute the border width as
   `int frame_width = (active window's width > 0) ? active window's width : <today's literal length, measured once>;`
2. Replace the hardcoded border/divider literal strings with a small
   helper that emits `frame_width` dashes (bounded, reusing `appendf`'s
   existing bounds-safety — no new overflow risk).
3. Do this for the outer frame border only first (the two lines shown
   above), not every internal divider — smallest visible slice, easiest
   to verify.
4. Leave body-line wrapping (`%-83.83s`-style width caps inside
   `append_project_probe_body`/`append_project_probe_scene_markup`)
   untouched for this step — those are a separate, smaller follow-on
   once the outer frame itself is proven to resize.

## KPIs for Path A

- **KPI B1 — zero visual regression at width=0. [DONE 2026-07-05]**
  Every project without a saved `WINDOW` section (i.e. all of them right
  now) renders an outer frame of exactly the same width as today,
  character for character.
  Verified standalone first (`frame_border_test.c` in the scratchpad):
  `resolve_frame_width(&w0, -1)` → 96, and `append_frame_border()`'s
  output was byte-for-byte identical (`strcmp() == 0`) to both original
  literal border strings at width 0. Then integrated into
  `wraith-alpha_manager.c` and confirmed with a full `gcc -Wall` build:
  exit 0, 0 errors, the same 78 pre-existing (unrelated) warnings as the
  KPI A1 build — nothing new introduced.
- **KPI B2 — visible resize with a saved width. [DONE 2026-07-05]**
  Hand-editing one test project's `project.pdl` to add
  `WINDOW | width | <some other number>` produces a visibly
  different-width outer frame the next time that window opens — proving
  the KPI A1 data path reaches the screen.
  Verified standalone: `resolve_frame_width(&w120, -1)` → 120,
  `append_frame_border()` correctly produced a 120-character top and
  bottom border (both scaling the dash-fill, title border keeping its
  `"WRAITH DESKTOP GUI"` label intact). Real end-to-end confirmation
  (actually opening a window in a running Wraith session with a saved
  `project.pdl` width and watching it render) is still open — this KPI
  is verified at the logic level, not yet eyeballed live in the running
  app.

### What actually landed (`wraith-alpha_manager.c`)

- `resolve_frame_width(const Window *window, int layout_declared_width)`
  — the 3-tier precedence function described above.
- `append_frame_border(char *out, size_t size, const char *label, int width)`
  — builds one `<br/>`-terminated border line of exactly `width` chars
  (`"+-LABEL---+"` when `label` is given, `"+-------+"` when NULL),
  appended via the same `appendf()` convention every other line in
  `build_desktop_shell_markup()` already uses.
- Both call sites in the active-window path of `build_desktop_shell_markup()`
  (top border, previously a literal with `"WRAITH DESKTOP GUI"` baked
  in; bottom border, previously a plain literal) now call
  `append_frame_border(out, size, ..., resolve_frame_width(window, -1))`.
  The separate `if (!window)` early-return branch (taskbar-only view,
  no window to read geometry from) was deliberately left untouched —
  out of scope per this doc's own Non-Goals.

## Non-goals for this doc's scope

- Not attempting `x`/`y` placement yet (no concept to hook into at this
  layer — see above).
- Not attempting true simultaneous multi-window visible rendering (still
  one active window's content shown at a time, per the existing model).
- Not starting Path B's implementation — investigation/plan only, and
  only after Path A is confirmed working.
