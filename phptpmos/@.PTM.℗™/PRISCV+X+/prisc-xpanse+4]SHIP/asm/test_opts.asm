# Simple Menu Demo - Options Menu
# 1 = Stay in Options, 2 = Back to Main
# layout_op rd, menu_id

start:
    addi x1, x0, 2        # x1 = menu_id (2 = options menu)
    layout_op x2, x1      # show menu, result in x2
    
    # Check selection (x2 = user input)
    addi x3, x2, -1       # x3 = selection - 1
    beq x3, x0, stay_opts # if x3 == 0, selection was 1
    
    addi x3, x2, -2       # x3 = selection - 2  
    beq x3, x0, go_main   # if x3 == 0, selection was 2
    
    # Default: stay
    j stay_opts

stay_opts:
    addi x4, x0, 2
    out x4                # OUT: 2 = stayed in options
    halt

go_main:
    addi x4, x0, 1
    out x4                # OUT: 1 = went to main  
    halt
