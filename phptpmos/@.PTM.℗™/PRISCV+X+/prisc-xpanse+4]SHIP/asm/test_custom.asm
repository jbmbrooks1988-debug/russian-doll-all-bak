# Test custom ops with array argument
player_pos int 10,20,30

start:
    lw x1, 0(x0)      # x1 = 10
    lw x2, 1(x0)      # x2 = 20  
    lw x3, 2(x0)      # x3 = 30
    sw x1, 100(x0)    # store x at mem[100]
    sw x2, 101(x0)    # store y at mem[101]
    sw x3, 102(x0)    # store z at mem[102]
    addi x4, x0, 100  # x4 points to array at mem[100]
    move_player x4
    halt
