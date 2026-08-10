livedesk-taskbar nav harness
============================
Purpose: prove tp_taskbar.c's real keyboard/mouse navigation (strip
buttons, popup submenus, the unified [>] cursor) works end-to-end via
real, autonomously-injected X11 events - no human at the keyboard.

Built directly out of the 2026-08-09 popup-keyboard-focus investigation
(!.HOUSE_STDS.md §F-19) - this is the same key_injector used to diagnose
and confirm that bug, kept here as a real, repeatable regression check
instead of a one-off debugging tool.

WHAT IT DOES (button.sh demo)
------------------------------
1. Builds the real taskbar binary + this harness's own key_injector.
2. Kills any stale taskbar, wipes shared/debug state, launches a fresh
   real taskbar instance against the live house.
3. Locates the real strip window on screen (xwininfo), clicks the HQ
   button, presses Down inside the popup, presses Escape - via REAL
   XTest-injected X11 events (XTestFakeKeyEvent/XTestFakeButtonEvent),
   genuinely indistinguishable from physical input to the receiving
   client.
4. Arms strip navigation (right-click) and presses Right twice.
5. Asserts against the taskbar's OWN real per-frame draw logs
   (#.desktop/tp_taskbar_debug/{strip,popup}_frame_log.txt) that focus
   actually moved as expected at each step - not just that the injector
   ran without erroring.
6. Confirms a real RGB frame receipt exists (strip_frame.raw +
   .receipt.txt) - proof a human/agent could visually verify the result
   too, not just infer it from text logs.

WHY THIS EXISTS - the general lesson, not just this one bug
--------------------------------------------------------------
tp_taskbar.c is a raw-Xlib program (reads real X11 events via
XNextEvent(), not a pieces/keyboard/history.txt-polling loop) - the
house's usual k3 file-injection testing method
(_.0.aigent-testing-k3.txt) does not apply to it. This harness is the
equivalent black-box interface for that architecture family: an XTest
injector tailing a plain text file, real events at the X server level.
See _.0.aigent-testing-k3.txt's own SCOPE note (added 2026-08-09) for
which method applies to which program family.

RUN
---
  bash button.sh demo

Proof (numbered step files, pass/fail summary) lands in
proof/harness-<timestamp>/ - same convention as every other harness in
%.harnesses/.

FILES
-----
  ops/key_injector.c    - the injector itself (vendored here, not
                           dependent on any session's ephemeral tmp
                           storage)
  scenarios/demo_strip_and_popup_nav.sh  - the one real scenario
  button.sh              - demo | help

RELATED
-------
  &.widgits/livedesk-taskbar/README.md   - the taskbar widget's own docs
  !.HOUSE_STDS.md §F-19                  - the bug this harness proves
                                            is fixed, and the general
                                            override-redirect-popup-focus
                                            rule for this house
  yz.muchiverse/a8-cc-++fix.md           - fuller 2026-08-09 session log
