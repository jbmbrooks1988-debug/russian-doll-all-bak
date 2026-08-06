/* crypt_autostart - real house-wide autostart daemon, 2026-08-05.
 * Reads $.crypts/autostart.pdl (real SECTION|KEY|VALUE format, same
 * convention every other .pdl in this house uses):
 *   STATE   | enabled    | 1|0          - real on/off toggle
 *   MOUNT   | uuid       | <disk UUID>  - a real volume to auto-mount
 *   MOUNT   | mountpoint | <path>       -   (each MOUNT pair is two
 *                                            consecutive rows: uuid then
 *                                            mountpoint, applied together)
 *   LAUNCH  | <name>     | <real shell command>
 *
 * Direct instruction: "a sort of 'c' auto start script, that will
 * autostart whatever programs user wants (in .pdl)... it should open
 * asa&ava and muchi-rancher; additionally it needs to auto mount the
 * 310 volume."
 *
 * Real, idempotent: mounting an already-mounted volume or launching
 * when `enabled=0` are both safe no-ops, not errors - this can be
 * re-run manually (via button.sh) as many times as you like.
 *
 * Usage: crypt_autostart.+x [pdl_path]
 *   pdl_path defaults to <this binary's own house-relative
 *   $.crypts/autostart.pdl>, resolved via /proc/self/exe so it works
 *   regardless of cwd (real XDG autostart invokes this with an
 *   unpredictable cwd).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>

#define PATH_BUF 4352
#define MAX_LINE 4352

static void dirname_step(const char *in, char *out, size_t out_sz) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", in);
    char *d = dirname(tmp);
    snprintf(out, out_sz, "%s", d);
}

/* This binary always lives at <house_root>/$.crypts/ops/+x/
 * crypt_autostart.+x - same real /proc/self/exe self-location technique
 * already proven this session (tp_desktop_window.c's own
 * resolve_livedesk_paths()). */
static void resolve_default_pdl(char *out, size_t out_sz) {
    char self_path[PATH_BUF];
    ssize_t slen = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (slen <= 0) { snprintf(out, out_sz, "autostart.pdl"); return; }
    self_path[slen] = '\0';
    char step[PATH_BUF];
    dirname_step(self_path, step, sizeof(step)); /* .../ops/+x */
    dirname_step(step, step, sizeof(step));      /* .../ops */
    dirname_step(step, step, sizeof(step));      /* .../$.crypts */
    snprintf(out, out_sz, "%s/autostart.pdl", step);
}

static int is_mounted(const char *mountpoint) {
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char dev[PATH_BUF], mp[PATH_BUF];
        if (sscanf(line, "%s %s", dev, mp) == 2 && strcmp(mp, mountpoint) == 0) {
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

int main(int argc, char **argv) {
    char pdl_path[PATH_BUF];
    if (argc > 1) snprintf(pdl_path, sizeof(pdl_path), "%s", argv[1]);
    else resolve_default_pdl(pdl_path, sizeof(pdl_path));

    FILE *f = fopen(pdl_path, "r");
    if (!f) {
        fprintf(stderr, "crypt_autostart: cannot open %s\n", pdl_path);
        return 1;
    }

    int enabled = 1;
    char mount_uuid[128] = "";
    char launch_cmds[64][MAX_LINE];
    char launch_names[64][64];
    int n_launch = 0;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) == 0 && strstr(line, "enabled")) {
            char *v = strrchr(line, '|');
            if (v) enabled = atoi(v + 1);
        } else if (strncmp(line, "MOUNT", 5) == 0 && strstr(line, "uuid")) {
            char *p = strchr(line, '|'); p = p ? strchr(p + 1, '|') : NULL;
            if (p) {
                p++;
                while (*p == ' ') p++;
                snprintf(mount_uuid, sizeof(mount_uuid), "%s", p);
                mount_uuid[strcspn(mount_uuid, "\r\n")] = '\0';
            }
        } else if (strncmp(line, "MOUNT", 5) == 0 && strstr(line, "mountpoint")) {
            char mp[PATH_BUF];
            char *p = strchr(line, '|'); p = p ? strchr(p + 1, '|') : NULL;
            if (p && mount_uuid[0]) {
                p++;
                while (*p == ' ') p++;
                snprintf(mp, sizeof(mp), "%s", p);
                mp[strcspn(mp, "\r\n")] = '\0';
                if (!is_mounted(mp)) {
                    char cmd[PATH_BUF * 2];
                    snprintf(cmd, sizeof(cmd),
                             "udisksctl mount -b /dev/disk/by-uuid/'%s' >/dev/null 2>&1",
                             mount_uuid);
                    int rc = system(cmd);
                    printf("crypt_autostart: mount UUID=%s -> %s (rc=%d)\n", mount_uuid, mp, rc);
                } else {
                    printf("crypt_autostart: %s already mounted, skipping\n", mp);
                }
                mount_uuid[0] = '\0';
            }
        } else if (strncmp(line, "LAUNCH", 6) == 0 && n_launch < 64) {
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ') p++;
            char *end = strchr(p, '|');
            if (!end) continue;
            char *label_end = end;
            while (label_end > p && label_end[-1] == ' ') label_end--;
            size_t nlen = (size_t)(label_end - p);
            if (nlen >= sizeof(launch_names[0])) nlen = sizeof(launch_names[0]) - 1;
            memcpy(launch_names[n_launch], p, nlen);
            launch_names[n_launch][nlen] = '\0';

            char *v = end + 1;
            while (*v == ' ') v++;
            v[strcspn(v, "\r\n")] = '\0';
            char *v_end = v + strlen(v);
            while (v_end > v && v_end[-1] == ' ') v_end--;
            *v_end = '\0';
            snprintf(launch_cmds[n_launch], sizeof(launch_cmds[0]), "%s", v);
            n_launch++;
        }
    }
    fclose(f);

    if (!enabled) {
        printf("crypt_autostart: disabled (STATE|enabled|0) - no-op\n");
        return 0;
    }

    for (int i = 0; i < n_launch; i++) {
        char cmd[MAX_LINE * 2];
        snprintf(cmd, sizeof(cmd), "setsid nohup %s >/dev/null 2>&1 &", launch_cmds[i]);
        int rc = system(cmd);
        printf("crypt_autostart: launch '%s' (rc=%d)\n", launch_names[i], rc);
    }

    return 0;
}
