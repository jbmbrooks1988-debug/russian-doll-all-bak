/* move_player custom op source
 * Usage: ./+x/move_player.+x <mem_addr>
 * Reads player xyz position from memory[addr], mem[addr+1], mem[addr+2]
 */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mem_addr>\n", argv[0]);
        return 1;
    }
    
    int addr = atoi(argv[1]);
    
    /* In a real implementation, the emulator would pass memory data
     * For now, we just print the address */
    printf("move_player: reading position from mem[%d]\n", addr);
    
    return 0;
}
