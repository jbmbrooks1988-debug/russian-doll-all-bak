# Example variable declarations
player_id int 1234
player_name char "duck"
player_position array 1,2,3

# Example program using variables and custom ops
start:
    lw x1, 0(x0)      # load player_id (1234) into x1
    out x1            # print player_id
    lw x2, 4(x0)      # load player_name pointer into x2
    out x2            # print name pointer
    lw x3, 8(x0)      # load player_position[0] (1) into x3
    out x3            # print position x
    halt
