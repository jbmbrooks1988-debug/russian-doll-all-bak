# Test variables
player_id int 1234
player_x int 100
player_y int 200

# Test program
start:
    lw x1, 0(x0)      # load player_id (1234) from mem[0]
    out x1
    lw x2, 1(x0)      # load player_x (100) from mem[1]
    out x2
    lw x3, 2(x0)      # load player_y (200) from mem[2]
    out x3
    halt
