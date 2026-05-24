# Debug Menu Demo
start:
    addi x1, x0, 2        # show options menu
    layout_op x1          # result in x0
    
    out x0                # debug: show selection
    
    addi x3, x0, -1
    out x3                # debug: show x3 after subtract 1
    
    beq x3, x0, stay_opts
    
    addi x3, x0, -2
    out x3                # debug: show x3 after subtract 2
    
    beq x3, x0, go_main
    
    j stay_opts

stay_opts:
    addi x4, x0, 2
    out x4
    halt

go_main:
    addi x4, x0, 1
    out x4
    halt
