# Test layout_op - menu with stored result
# This test shows how layout_op integrates with emulator

# Memory layout:
# mem[50] = last menu selection (written by layout_op via temp file)

start:
    # Simulate a previous menu selection stored in memory
    addi x1, x0, 2        # x1 = 2 (user selected "Load Game")
    sw x1, 50(x0)         # store selection at mem[50]
    
    # Read the selection and branch
    lw x2, 50(x0)         # x2 = menu selection
    
    # Branch based on selection
    addi x3, x0, 1
    beq x2, x3, start_game
    
    addi x3, x0, 2
    beq x2, x3, load_game
    
    addi x3, x0, 3
    beq x2, x3, options
    
    # Default (exit)
    addi x4, x0, 0
    out x4                # OUT: 0 (exit)
    halt

start_game:
    addi x4, x0, 1
    out x4                # OUT: 1 (started game)
    halt

load_game:
    addi x4, x0, 2
    out x4                # OUT: 2 (loaded game)
    halt

options:
    addi x4, x0, 3
    out x4                # OUT: 3 (options)
    halt
