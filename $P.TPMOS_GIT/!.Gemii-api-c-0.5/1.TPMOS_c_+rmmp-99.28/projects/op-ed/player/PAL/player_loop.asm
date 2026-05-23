# player_loop.asm - OP-ED Game Player Orchestrator
# Purpose: Minimal loop for hero movement and interaction

# Registers:
# r1: current key
# r2: scratch
# r3: history position
# r10: sleep time (16667 for 60 FPS)

li r10, 16667
li r3, 0

loop:
    # 1. Poll for new keys in history
    # (Assuming player_app history is used for now)
    read_history r1, r3
    beq r1, x0, idle_loop

    # 2. Process keys for movement
    # 'w'=119, 's'=115, 'a'=97, 'd'=100
    
    addi r2, r1, -119
    beq r2, x0, move_up
    
    addi r2, r1, -115
    beq r2, x0, move_down
    
    addi r2, r1, -97
    beq r2, x0, move_left
    
    addi r2, r1, -100
    beq r2, x0, move_right
    
    # ' '=32 (Space for interaction)
    addi r2, r1, -32
    beq r2, x0, interact

    j loop

move_up:
    exec ./pieces/ops/play-ops/+x/move_entity.+x hero w fuzz-op-mirror
    hit_frame
    j loop

move_down:
    exec ./pieces/ops/play-ops/+x/move_entity.+x hero s fuzz-op-mirror
    hit_frame
    j loop

move_left:
    exec ./pieces/ops/play-ops/+x/move_entity.+x hero a fuzz-op-mirror
    hit_frame
    j loop

move_right:
    exec ./pieces/ops/play-ops/+x/move_entity.+x hero d fuzz-op-mirror
    hit_frame
    j loop

interact:
    # Trigger interact on pet_01 if nearby
    # (Simplified for now, just trigger it)
    exec ./pieces/system/prisc/prisc+x projects/op-ed/games/fuzz-op-mirror/scripts/pet_interact.asm
    hit_frame
    j loop

idle_loop:
    sleep r10
    j loop
