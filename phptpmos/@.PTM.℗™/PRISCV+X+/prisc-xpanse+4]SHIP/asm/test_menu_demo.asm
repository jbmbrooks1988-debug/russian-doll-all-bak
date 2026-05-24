# Full Interactive Menu Demo
# Main Menu (id=1): 1=Start Game, 2=Options, 3=Exit
# Options Menu (id=2): 1=Stay in Options, 2=Back to Main
#
# Usage:
#   printf "2\n2\n1\n3\n" > /tmp/prisc_input.txt
#   rm -f /tmp/prisc_input_count.txt
#   ./prisc+x test_menu_demo.asm 2>&1
#
# Expected output:
#   Menu renders, navigates: Main→Options→Main→Start→Exit
#   OUT: 100 (started game)
#   OUT: 0 (exiting)

start:
    addi x1, x0, 1        # start with main menu (id=1)

menu_loop:
    layout_op x2, x1      # show menu (x1), result in x2
    
    # Check which menu we're in
    addi x3, x1, -1
    beq x3, x0, handle_main
    
    addi x3, x1, -2
    beq x3, x0, handle_options
    
    j menu_loop

handle_main:
    # Main menu: x2 = selection
    # 1=Start, 2=Options, 3=Exit, 0=Quit
    beq x2, x0, exit_game     # 0 = quit
    
    addi x3, x2, -1
    beq x3, x0, start_game
    
    addi x3, x2, -2
    beq x3, x0, go_options
    
    addi x3, x2, -3
    beq x3, x0, exit_game
    
    j exit_game

handle_options:
    # Options menu: x2 = selection
    # 1=Stay, 2=Back, 0=Quit
    beq x2, x0, exit_game     # 0 = quit
    
    addi x3, x2, -1
    beq x3, x0, opt_stay
    
    addi x3, x2, -2
    beq x3, x0, go_main
    
    j exit_game

start_game:
    addi x4, x0, 100
    out x4                # OUT: 100 = started game!
    addi x1, x0, 1        # back to main menu
    j menu_loop

go_options:
    addi x1, x0, 2        # switch to options menu
    j menu_loop

go_main:
    addi x1, x0, 1        # switch to main menu
    j menu_loop

opt_stay:
    addi x1, x0, 2        # stay in options menu
    j menu_loop

exit_game:
    addi x4, x0, 0
    out x4                # OUT: 0 = exiting
    halt
