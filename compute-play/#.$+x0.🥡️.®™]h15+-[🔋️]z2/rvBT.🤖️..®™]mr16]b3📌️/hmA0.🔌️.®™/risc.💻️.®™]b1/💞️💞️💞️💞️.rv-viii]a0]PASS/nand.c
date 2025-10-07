#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s bitwidth a b\n", argv[0]);
        return 1;
    }
    int bitwidth = atoi(argv[1]);
    unsigned long a = strtoul(argv[2], NULL, 10);
    unsigned long b = strtoul(argv[3], NULL, 10);
    unsigned long result = ~(a & b);
    if (bitwidth == 2) result &= 0x3;
    else if (bitwidth == 4) result &= 0xF;
    else if (bitwidth == 8) result &= 0xFF;
    else if (bitwidth == 16) result &= 0xFFFF;
    else if (bitwidth == 32) result &= 0xFFFFFFFF;
    printf("%lu\n", result);
    return 0;
}
