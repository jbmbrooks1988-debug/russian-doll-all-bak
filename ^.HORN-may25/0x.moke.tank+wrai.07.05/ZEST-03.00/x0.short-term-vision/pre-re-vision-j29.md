# Pre-Re-Vision J29

Date: 2026-06-29
Status: Pre-sprint alignment note before the next Wraith/XO/editor push.

==================================================
1. PURPOSE
==================================================

This doc is a place to organize the current shift in priority before starting a new sprint.

The goal is not to freeze a final architecture.

The goal is to:

- restate where Wraith now stands
- clarify what should happen next
- reconcile `wraith-ed`, `fs`, `0x-pet`, and `x0.moke-` questions
- identify small pre-fixes worth doing before new project momentum starts

==================================================
2. CURRENT FEELING OF THE SYSTEM
==================================================

Wraith is no longer at the "prove a shell exists" stage.

It now feels close to a usable host environment.

That changes the right strategy:

- less abstract framework polishing
- more real project/app/game work
- fix engine seams in context

This is especially true because:

- ASCII/GL switching exists
- project hosting works
- frame-debug auditability has improved
- `fs` has become a real proving lane

So the next sprint should probably be shaped around real use, not endless shell theory.

==================================================
3. MAJOR TOPICS NOW COMPETING FOR ATTENTION
==================================================

The current pressure seems to come from four directions:

1. `wraith-ed` should become meaningfully more capable
2. Wraith/XO/`0x-pet` vision may need to be revisited
3. the `fs` / launcher / nested-folder model wants another pass
4. `x0.moke-` transfer timing needs to be kept straight

These are related enough that they should not be planned in isolation.

==================================================
4. WRAITH-ED: WHAT THE NEXT VERSION SHOULD MEAN
==================================================

Current directional desire:

- make `wraith-ed` as capable as `op-ed`
- but keep it in the Wraith-hosted pattern
- and add the option to toggle between:
  - 2D `game-map` view
  - 3D `game-map` / canvas-style view

This is a good next target because it combines:

- editor growth
- file-backed artifacts
- `game_map` semantics
- future `chtpmgl` / GL media pressure
- project-hosted rather than manager-hardcoded behavior

My current understanding is:

`wraith-ed` should not become "op-ed copied into Wraith with different branding."

It should become:

- Wraith-hosted editor
- sharing real reusable ops where possible
- exposing both 2D and 3D/canvas-like world views over the same project truth

That makes it a better bridge toward:

- game creation
- 3D previews
- enclosure editing later
- agents/PAL reusing the same file-backed editing seams

==================================================
5. FS / LAUNCHER / NESTED FOLDER QUESTION
==================================================

This is the biggest practical design question right now.

Desired behavior:

- if an entry is a Wraith project, launch it
- if an entry is just a directory, open it as a nested submenu / deeper folder view
- nested project folders should be allowed for organization
- the long-term vision is one flexible filesystem + launcher situation, not separate disconnected flows

That is the right long-term shape.

But there are two different places this behavior could live:

## Option A: change global Wraith project detection/launcher flow first

Pros:
- closer to the final "one launcher/fs truth" vision

Cons:
- touches compile/discovery assumptions earlier
- mixes nesting semantics with launcher semantics immediately
- more likely to destabilize the current project scan/compile flow

## Option B: make `fs` smarter first

Pros:
- lower-risk proving ground
- easier to test
- gives us the exact user behavior we want sooner
- allows project launch + plain-dir traversal inside one tool immediately

Cons:
- global launcher remains simpler/flatter for a while

My current recommendation:

Start with Option B.

Meaning:

- leave the regular project detection/launcher flow mostly as-is for now
- teach `fs` to distinguish:
  - Wraith project
  - plain directory
  - ordinary file
- launching a project from `fs` is acceptable and probably the better near-term modification

Reason:

The current launcher still has value as the stable direct project surface.

`fs` can become the flexible proving ground for the unified launcher/filesystem future without forcing that full migration immediately.

==================================================
6. WHAT "ONE FLEXIBLE FS / LAUNCHER" REALLY MEANS
==================================================

The long-term vision seems to be:

- one navigable environment
- directories for organization
- projects as launchable artifacts
- files as editable/selectable artifacts
- not separate fake categories with separate mental models

That implies a future unified entry model like:

1. directory
   - opens deeper view
2. project
   - launches
3. file
   - opens/inspects/edits according to context
4. enclosure/bundle
   - opens in the relevant host/editor flow

This is probably the right final mental model for Wraith.

But it does not all need to be implemented before the next sprint.

==================================================
7. 0X-PET / X0-PET / WRAITH / MOKE REVISIT
==================================================

This likely is a good time to re-discuss the vision, but not to split implementation across both trees yet.

What still seems true:

- enclosure should remain the sovereign artifact
- `0x-pet` and Wraith should become different hosts/front doors over the same truth
- editor-pattern thinking is still the right way to unify enclosure selection, editing, and control

What may be shifting now:

- if `wraith-ed` becomes stronger, it may become the practical place where enclosure-style editing/control is first proven
- `fs` becoming more flexible may also reduce the pressure to invent a separate chooser path too early

What should not change:

- do not start actively implementing the new Wraith lane in both:
  - `1.TPMOS`
  - `x0.moke-pet-project-04.03`

Current rule should remain:

- prove the current Wraith lane first in `1.TPMOS`
- then copy/adapt into `x0.moke-`

==================================================
8. SHOULD WE TRANSFER CURRENT WRAITH INTO X0.MOKE NOW?
==================================================

My answer is still: not yet.

Reason:

- Wraith just became coherent enough to accelerate
- this is the wrong moment to fork attention into synchronization work
- `wraith-ed`, media support, and smarter `fs` behavior should be proven first

The right condition for transfer is:

- current Wraith lane is stable enough that copying it will save future work instead of duplicating active churn

We are close, but not quite at the point where I would spend the next sprint on transfer.

==================================================
9. WHAT PRE-FIXES ARE WORTH DOING BEFORE THE NEXT SPRINT
==================================================

Only a few pre-fixes seem worth prioritizing before new project momentum:

1. `fs` should identify project-vs-dir-vs-file more explicitly
2. `fs` should launch Wraith projects when the chosen entry is a project
3. plain directories in `fs` should descend, not pretend to be launch targets
4. `wraith-ed` should get a clarified next-sprint target before broad implementation starts

Everything else should probably be fixed only when real project work exposes it.

==================================================
10. RECOMMENDED NEXT-SPRINT ORDER
==================================================

My current recommended order:

1. update vision/alignment docs
2. improve `fs` entry typing + project launch behavior
3. start the next real `wraith-ed` capability push
4. keep 2D/3D `game-map` toggle in view as part of that editor push
5. defer `x0.moke-` transfer until the lane settles more
6. revisit enclosure/`0x-pet` integration after `wraith-ed` gains more real editing power

==================================================
11. SHORT VERSION
==================================================

The system feels ready to stop nesting in framework work all the time.

The likely right move is:

- let `fs` become the first flexible launcher/filesystem hybrid
- let `wraith-ed` become the next serious capability push
- keep enclosure/`0x-pet` vision alive in the docs
- do not split active implementation into `x0.moke-` yet

That gives us both:

- immediate practical progress
- cleaner information for the next real sprint
