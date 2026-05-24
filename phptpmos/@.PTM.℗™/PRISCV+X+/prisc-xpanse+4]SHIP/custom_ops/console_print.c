/* console_print custom op source
 * Usage: ./+x/console_print.+x value
 * Prints input to console
 */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <value>\n", argv[0]);
        return 1;
    }
    
    int value = atoi(argv[1]);
    
    printf("console_print: %d\n", value);
    
    /* Return value can be captured by emulator */
    printf("%d\n", value);
    
    return 0;
}
