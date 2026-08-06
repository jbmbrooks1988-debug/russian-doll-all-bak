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
 * REAL 2026-08-06: each successful `run` first quits the current
 * livedesk session (CLOSE relays + SIGTERM taskbar/entities for this
 * house_root, clear open/claims) so `$` on the taskbar restarts clean
 * instead of stacking duplicate processes.
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
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>

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


/* Resolve <house_root> from pdl at <house_root>/$.crypts/autostart.pdl */
static void resolve_house_root_from_pdl(const char *pdl_path, char *out, size_t out_sz) {
    char step[PATH_BUF];
    dirname_step(pdl_path, step, sizeof(step)); /* .../$.crypts */
    dirname_step(step, out, out_sz);             /* .../house_root */
}

static int pid_alive(pid_t pid) {
    if (pid <= 1) return 0;
    return kill(pid, 0) == 0 || errno != ESRCH;
}

/* REAL 2026-08-06, user: when $ / button.sh run fires, quit current
 * livedesk first so we don't stack old tp_desktop_window/taskbar on
 * top of new binaries (focus-recovery restarts). Graceful CLOSE via
 * interact_relay, then SIGTERM leftovers, clear open/claims registries. */
static void quit_current_livedesk(const char *house_root) {
    char open_path[PATH_BUF];
    char claims_path[PATH_BUF];
    char tbar_pid_path[PATH_BUF];
    snprintf(open_path, sizeof(open_path), "%s/#.desktop/livedesk_open.txt", house_root);
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk_nav_claims.txt", house_root);
    snprintf(tbar_pid_path, sizeof(tbar_pid_path), "%s/#.desktop/livedesk_taskbar.pid", house_root);

    printf("crypt_autostart: quitting current livedesk before re-launch\n");

    /* 1) Graceful CLOSE to every registered entity package. */
    FILE *of = fopen(open_path, "r");
    if (of) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), of)) {
            char *pp = strstr(line, "PATH=");
            char *pidp = strstr(line, "PID=");
            if (pp) {
                char path[PATH_BUF];
                snprintf(path, sizeof(path), "%s", pp + 5);
                path[strcspn(path, "\r\n")] = '\0';
                /* strip trailing junk if any after path (shouldn't be) */
                char relay[PATH_BUF];
                snprintf(relay, sizeof(relay), "%s/interact_relay.txt", path);
                FILE *rf = fopen(relay, "w");
                if (rf) { fprintf(rf, "CLOSE\n"); fclose(rf); }
                printf("crypt_autostart: CLOSE -> %s\n", path);
            }
            if (pidp) {
                pid_t pid = (pid_t)atoi(pidp + 4);
                if (pid_alive(pid)) {
                    /* give CLOSE a moment; SIGTERM after loop */
                    (void)pid;
                }
            }
        }
        fclose(of);
    }

    /* 2) Taskbar pid file. */
    {
        FILE *pf = fopen(tbar_pid_path, "r");
        if (pf) {
            int tpid = 0;
            if (fscanf(pf, "%d", &tpid) == 1 && tpid > 1 && pid_alive((pid_t)tpid)) {
                kill((pid_t)tpid, SIGTERM);
                printf("crypt_autostart: SIGTERM taskbar pid=%d\n", tpid);
            }
            fclose(pf);
        }
    }

    /* 3) Brief wait for graceful CLOSE to take effect. */
    usleep(400000);

    /* 4) SIGTERM any still-alive PIDs from livedesk_open.txt. */
    of = fopen(open_path, "r");
    if (of) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), of)) {
            char *pidp = strstr(line, "PID=");
            if (!pidp) continue;
            pid_t pid = (pid_t)atoi(pidp + 4);
            if (pid_alive(pid)) {
                kill(pid, SIGTERM);
                printf("crypt_autostart: SIGTERM entity pid=%d\n", (int)pid);
            }
        }
        fclose(of);
    }

    /* 5) Sweep: any remaining tp_desktop_window / tp_taskbar whose
     * cmdline contains this house_root (catches stragglers not in open). */
    {
        DIR *d = opendir("/proc");
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (!isdigit((unsigned char)ent->d_name[0])) continue;
                pid_t pid = (pid_t)atoi(ent->d_name);
                if (pid <= 1 || pid == getpid()) continue;
                char cpath[64];
                snprintf(cpath, sizeof(cpath), "/proc/%d/cmdline", (int)pid);
                FILE *cf = fopen(cpath, "r");
                if (!cf) continue;
                char cmd[PATH_BUF * 2];
                size_t n = fread(cmd, 1, sizeof(cmd) - 1, cf);
                fclose(cf);
                if (n == 0) continue;
                cmd[n] = '\0';
                /* cmdline is NUL-separated; turn NULs into spaces for strstr */
                for (size_t i = 0; i < n; i++) if (cmd[i] == '\0') cmd[i] = ' ';
                int is_tb = strstr(cmd, "tp_taskbar") != NULL;
                int is_dw = strstr(cmd, "tp_desktop_window") != NULL;
                if (!is_tb && !is_dw) continue;
                if (!strstr(cmd, house_root)) continue;
                if (pid_alive(pid)) {
                    kill(pid, SIGTERM);
                    printf("crypt_autostart: SIGTERM sweep pid=%d (%s)\n",
                           (int)pid, is_tb ? "taskbar" : "desktop_window");
                }
            }
            closedir(d);
        }
    }

    usleep(200000);

    /* 6) Clear registries so next launch starts clean (no phantom [N]). */
    {
        FILE *wf = fopen(open_path, "w");
        if (wf) fclose(wf);
        wf = fopen(claims_path, "w");
        if (wf) fclose(wf);
        /* leave taskbar pid file empty / gone */
        unlink(tbar_pid_path);
    }
    printf("crypt_autostart: livedesk quit complete\n");
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

    /* Always quit existing livedesk before re-launch ($ shortcut / run). */
    {
        char house_root[PATH_BUF];
        resolve_house_root_from_pdl(pdl_path, house_root, sizeof(house_root));
        if (house_root[0]) quit_current_livedesk(house_root);
    }

    for (int i = 0; i < n_launch; i++) {
        char cmd[MAX_LINE * 2];
        snprintf(cmd, sizeof(cmd), "setsid nohup %s >/dev/null 2>&1 &", launch_cmds[i]);
        int rc = system(cmd);
        printf("crypt_autostart: launch '%s' (rc=%d)\n", launch_names[i], rc);
    }

    return 0;
}
