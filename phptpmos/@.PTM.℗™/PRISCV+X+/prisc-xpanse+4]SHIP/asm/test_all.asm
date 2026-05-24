# Comprehensive test of pRISC+X features

# Variables (auto-allocated to memory)
player_id int 1234
player_x int 10
player_y int 20
player_z int 30

start:
    # Test basic ops
    addi x1, x0, 42
    out x1            # OUT: 42
    
    # Test variables
    lw x2, 0(x0)      # load player_id from mem[0]
    out x2            # OUT: 1234
    
    # Test custom op with array pointer
    lw x3, 1(x0)      # x3 = player_x (10)
    lw x4, 2(x0)      # x4 = player_y (20)
    lw x5, 3(x0)      # x5 = player_z (30)
    sw x3, 200(x0)    # store position array at mem[200]
    sw x4, 201(x0)
    sw x5, 202(x0)
    addi x6, x0, 200  # x6 points to array
    move_player x6    # call custom op
    
    # Test console_print custom op
    console_print x1
    
    halt
