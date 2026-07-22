# angler-empires (pure C / TPMOS-family)

GCC C11 rebuild of the Angler Empires TTY game, on the same architecture as
`y0.mutaclsym+05.01/mutaclsym+1` (prisc+x + ops + pieces + optional GL mirror).


## Status — Phase R0 (scaffold)

- [x] Pure `gcc -std=c11` (no C++, no ncurses)
- [x] `system/`: keyboard_input, renderer, prisc+x, gl_mirror (optional)
- [x] Ops: move_player, end_turn, compose_frame, pdl_reader
- [x] Minimal `world_angler_home/map_start` + hero walkable 20x10 map
- [x] Footer legend; METHOD table ready for R2 digit-nav
- [ ] Items, choice nav, activity/talk/scene, full ANGLER EMPIRES floor, GL (R1-R4)

## Build / run

```bash
cd angler-empires
./button.sh compile
./button.sh check
./button.sh run
```

GL is **off by default** in R0 (`NO_GL` defaults to 1). When R4 lands
`compose_rgb_frame`, use `NO_GL=0 ./button.sh run`.

Controls (R0): wasd/arrows move, q quit.

## Headless smoke test

```bash
export PRISC_PROJECT_ROOT="$PWD"
./ops/+x/compose_frame.+x
./ops/+x/move_player.+x 100   # 'd'
./ops/+x/end_turn.+x
./ops/+x/compose_frame.+x
cat pieces/display/current_frame.txt
```
