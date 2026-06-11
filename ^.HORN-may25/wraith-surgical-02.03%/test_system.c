#include <stdlib.h>
#include <stdio.h>
int main() {
    int res = system("'ls -la'");
    printf("Result: %d\n", res);
    return 0;
}
