# Wraith 3D Cube Implementation Notes

This project validates 3D rendering only inside a `${game_map}` surface.

Do not use this path for normal windows, taskbars, buttons, debug rows, or CHTMGL widgets unless they explicitly contain a `${game_map}` surface.

## Existing Standard To Use

Cube object storage must follow the existing Piececraft/fuzz-op-gl piece convention:

```txt
pieces/cube_probe/artifact.txt
pieces/cube_probe/state.txt
```

The artifact is a stacked z-slice bitmask:

```txt
z0=FF,FF,FF,FF,FF,FF,FF,FF
...
z7=FF,FF,FF,FF,FF,FF,FF,FF
```

Each `zN` is a 2D slice. Each byte is a Y row. Each bit is an X cell.

## Required Semantic Export

The manager/RGB bridge should eventually emit a semantic object like:

```txt
PIECE_MODEL | id=cube_probe | role=zslice_piece | source=pieces/cube_probe/artifact.txt | surface=game_map | rot=15,35,0 | selected=true
CAMERA | mode=4 | x=0.00 | y=0.00 | z=0.00 | pitch=15.00 | yaw=0.00 | roll=0.00
```

ASCII should show an audit projection. It is not the source of full 3D truth.

## RGB Converter Work

Implement in `projects/wraith-alpha/plugins/wraith_rgb_daemon.c`, not in `wraith_gl.c`.

Current live status:
- The RGB daemon has a first-pass `role=zslice_piece` preview renderer.
- It draws a recognizable cube-ish stacked-face preview into the `${game_map}` surface.
- This is not yet the intended final cube renderer.
- It does not yet consume full camera/rotation metadata, build real cube faces from occupied cells, depth-sort faces, or emit complete cube projection receipts.

The converter should:
- load the artifact z-slices.
- turn occupied bits into cube cells/faces.
- apply `rot_x`, `rot_y`, `rot_z`.
- apply Piececraft camera state.
- project into the `${game_map}` surface bounds.
- rasterize shaded faces into `current_frame.rgba32`.
- write receipt data for source path, slice count, occupied cells, camera, rotation, projected bounds, draw order, and checksum.

## Controls

Reuse Piececraft command vocabulary:
- `CAMERA_MODE:n`
- `CAMERA_SET:x,y,z,pitch,yaw,roll`
- `CAMERA_MOVE:dx,dy,dz`
- `ROTATE_CUBE:axis,delta`

Current live status:
- Wraith Alpha can enter interact mode and logs `COMMAND: INTERACT` in `session/history.txt`.
- Camera/rotation controls do not work yet because this project does not yet have a project-owned input manager/op.

Next project-owned op:
- read `session/history.txt`
- only process controls while `session/state.txt` has `is_map_control=1`
- map arrows/WASD/ZX or explicit commands to the Piececraft camera vocabulary
- update `session/state.txt` and `pieces/cube_probe/state.txt`
- regenerate `session/wraith_body.txt` with current camera/rotation audit text
- leave `wraith-alpha_manager.c` generic
