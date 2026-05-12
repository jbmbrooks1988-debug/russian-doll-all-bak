#include <stdio.h>
#include <stdlib.h>

int main() {
    const char *path = "/home/no/Desktop/Piecemark-IT/中.SP_00.00/!.sp-inniϕ©+29/!.SP.all-writeez30-c0.4/8888.SP.CYOA-LAB-rw&b/1.TPMOS_c_+rmmp_98.54/pieces/chtpm/ops/+x/get_projects_op.+x";
    printf("Testing popen with unquoted path...\n");
    FILE *fp = popen(path, "r");
    if (fp) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), fp)) printf("%s", buf);
        pclose(fp);
    } else {
        perror("popen failed");
    }

    printf("\nTesting popen with quoted path...\n");
    char quoted_path[2048];
    snprintf(quoted_path, sizeof(quoted_path), "'%s'", path);
    fp = popen(quoted_path, "r");
    if (fp) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), fp)) printf("%s", buf);
        pclose(fp);
    } else {
        perror("popen failed");
    }

    return 0;
}
