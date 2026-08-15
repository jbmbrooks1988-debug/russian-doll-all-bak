#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <glob.h>

int main(void) {
    /* Find the house root by walking up from this binary's own real
     * install dir until a directory holding BOTH #.desktop/ and
     * &.widgits/ is found - same marker-walk khtpm_vars.sh uses, so no
     * hardcoded NNEST-* / yz.muchiverse paths (the old button_launcher
     * globbed for them and broke on any relocation). */
    char self_path[PATH_MAX];
    ssize_t slen = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (slen <= 0) {
        fprintf(stderr, "Could not resolve own path\n");
        return 1;
    }
    self_path[slen] = '\0';

    char step[PATH_MAX];
    snprintf(step, sizeof(step), "%s", self_path);
    char house_root[PATH_MAX];
    house_root[0] = '\0';
    for (;;) {
        char *slash = strrchr(step, '/');
        if (!slash || slash == step) break; /* reached /, give up */
        *slash = '\0';
        char desk[PATH_MAX], widg[PATH_MAX];
        snprintf(desk, sizeof(desk), "%s/#.desktop", step);
        snprintf(widg, sizeof(widg), "%s/&.widgits", step);
        if (access(desk, F_OK) == 0 && access(widg, F_OK) == 0) {
            snprintf(house_root, sizeof(house_root), "%s", step);
            break;
        }
    }
    if (house_root[0] == '\0') {
        fprintf(stderr, "Could not find house root\n");
        return 1;
    }

    char button_sh[PATH_MAX];
    snprintf(button_sh, sizeof(button_sh), "%s/$.crypts/button.sh", house_root);
    if (access(button_sh, F_OK) != 0) {
        fprintf(stderr, "Could not find button.sh\n");
        return 1;
    }

    char build_dir[PATH_MAX];
    snprintf(build_dir, sizeof(build_dir), "%s/*.monads/*.livedesk-taskbar/ops", house_root);

    char build_cmd[PATH_MAX * 2];
    snprintf(build_cmd, sizeof(build_cmd), "cd '%s' && bash build_khtpm_strip.sh >/dev/null 2>&1", build_dir);
    system(build_cmd);

    char *args[] = { "sh", button_sh, "run", NULL };
    execvp("sh", args);
    return 1;
}
