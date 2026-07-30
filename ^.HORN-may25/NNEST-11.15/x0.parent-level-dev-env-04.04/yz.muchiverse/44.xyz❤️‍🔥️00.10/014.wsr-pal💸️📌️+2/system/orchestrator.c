// orchestrator.c - TPMOS launcher for WSR-PAL
// Stock market simulation game orchestrator
// Uses fork/exec for all child processes (Bible §3)
// Process management: x0.moke 3-layer cascading kill pattern
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/file.h>
#include <dirent.h>

#define MAX_CHILDREN 8

static volatile int should_exit = 0;

/* === LAYER 1: Process Group Kill (fast path) === */

static void kill_process_group(void) {
    /* Send SIGTERM to entire process group (all forked children) */
    kill(0, SIGTERM);
    usleep(100000); /* 100ms grace period */
}

/* === LAYER 2: File-Backed PID Tracking (x0.moke HOLY Pattern) === */

static void log_pid(int pid, const char* name) {
    FILE* f = fopen("pieces/os/proc_list.txt", "a");
    if (!f) return;
    flock(fileno(f), LOCK_EX);
    fprintf(f, "%d %s\n", pid, name);
    fflush(f);
    fsync(fileno(f));
    flock(fileno(f), LOCK_UN);
    fclose(f);
}

static void kill_all_tracked(void) {
    FILE* f = fopen("pieces/os/proc_list.txt", "r");
    if (!f) return;
    char line[256];
    /* Phase 1: SIGTERM */
    while (fgets(line, sizeof(line), f)) {
        int pid; char name[128];
        if (sscanf(line, "%d %127s", &pid, name) == 2) {
            kill(pid, SIGTERM);
        }
    }
    fclose(f);
    usleep(200000);
    /* Phase 2: SIGKILL survivors */
    f = fopen("pieces/os/proc_list.txt", "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        int pid; char name[128];
        if (sscanf(line, "%d %127s", &pid, name) == 2) {
            kill(pid, SIGKILL);
            waitpid(pid, NULL, WNOHANG);
        }
    }
    fclose(f);
    /* Clear for next run */
    f = fopen("pieces/os/proc_list.txt", "w");
    if (f) fclose(f);
}

/* === LAYER 3: kill_all.sh Final Sweep (nuclear option) === */

static void run_final_kill_sweep(void) {
    pid_t pid = fork();
    if (pid == 0) {
        /* Redirect to /dev/null, run kill_all.sh */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/bash", "bash", "pieces/os/kill_all.sh", NULL);
        _exit(1);
    }
    if (pid > 0) {
        /* Wait up to 2 seconds for kill_all.sh to finish */
        int status;
        waitpid(pid, &status, WNOHANG);
    }
}

/* === SIGNAL HANDLER === */

void handle_signal(int sig) {
    (void)sig;
    should_exit = 1;
    
    fprintf(stderr, "\n[Orchestrator] Shutting down (signal %d)...\n", sig);
    
    /* 3-layer cascading kill (x0.moke pattern) */
    kill_process_group();    /* Layer 1: process group SIGTERM */
    kill_all_tracked();      /* Layer 2: file-backed PID sweep */
    run_final_kill_sweep();  /* Layer 3: kill_all.sh nuclear option */
    
    fprintf(stderr, "[Orchestrator] Cleanup complete.\n");
    _exit(0);
}

/* === PROCESS LAUNCH (fork/exec, Bible §3 compliant) === */

static pid_t launch(const char *path, const char *arg1, const char *arg2) {
    pid_t pid = fork();
    if (pid == 0) {
        if (arg2)
            execl(path, path, arg1, arg2, NULL);
        else if (arg1)
            execl(path, path, arg1, NULL);
        else
            execl(path, path, NULL);
        fprintf(stderr, "[Orchestrator] exec failed: %s\n", path);
        _exit(1);
    }
    if (pid > 0) {
        log_pid(pid, path);
        fprintf(stderr, "[Orchestrator] Launched %s (PID %d)\n", path, pid);
    }
    return pid;
}

/* === BINARY COMPILATION (fork/exec, Bible §3 compliant) === */

static void compile_single(const char *src, const char *dst, const char *extra_flags) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        char cmd[512];
        if (extra_flags)
            snprintf(cmd, sizeof(cmd), "gcc -o %s %s %s", dst, src, extra_flags);
        else
            snprintf(cmd, sizeof(cmd), "gcc -o %s %s", dst, src);
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

/* Compile ops/*.c → ops/+x/*.+x via loop (Bible §3 compliant) */
static void compile_ops(void) {
    DIR *d = opendir("ops");
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        size_t len = strlen(entry->d_name);
        if (len < 3 || strcmp(entry->d_name + len - 2, ".c") != 0) continue;

        char src[512], dst[512];
        snprintf(src, sizeof(src), "ops/%s", entry->d_name);

        /* Strip .c, replace with +x extension */
        char base[256];
        strncpy(base, entry->d_name, len - 2);
        base[len - 2] = '\0';
        snprintf(dst, sizeof(dst), "ops/+x/%s.+x", base);

        const char *flags = NULL;
        if (strstr(entry->d_name, "dump_rgb_png"))
            flags = "-Iops/lib -lm";
        else
            flags = "-lm";

        fprintf(stderr, "[Orchestrator] Compiling %s → %s\n", src, dst);
        compile_single(src, dst, flags);
    }
    closedir(d);
}

static void compile_binaries(void) {
    fprintf(stderr, "[Orchestrator] Compiling...\n");
    compile_single("system/prisc+x.c", "system/prisc+x", NULL);
    compile_single("system/keyboard_input.c", "system/keyboard_input", NULL);
    compile_single("system/renderer.c", "system/renderer", NULL);
    compile_single("system/chtpm_parser_pal.c", "system/chtpm_parser_pal",
                   "-Wno-unused-result -Wno-stringop-truncation");
    compile_single("system/chtpm_rgb_render.c", "system/chtpm_rgb_render", NULL);

    /* Best effort: gl_mirror needs OpenGL libs, skip if gcc fails */
    fprintf(stderr, "[Orchestrator] Compiling gl_mirror (best effort)...\n");
    compile_single("system/gl_mirror.c", "system/gl_mirror", "-lglut -lGL -lGLU");

    compile_ops();
}

/* === DIRECTORY SETUP (Bible §3 compliant) === */

static void ensure_directories(void) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/mkdir", "mkdir", "-p",
              "pieces/system", "pieces/display",
              "pieces/apps/player_app", "pieces/keyboard",
              "pieces/os", "ops/+x", NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

/* === ENSURE ENTITIES (fork/exec scripts/ensure_entities.sh) === */

static void run_ensure_entities(void) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/bash", "bash", "scripts/ensure_entities.sh", NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

/* === MAIN === */

int main(void) {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    fprintf(stderr, "[Orchestrator] Starting WSR-PAL from %s\n", cwd);

    /* Register signal handlers for 3-layer cleanup */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    compile_binaries();

    fprintf(stderr, "[Orchestrator] Initializing...\n");
    ensure_directories();
    run_ensure_entities();

    /* Clear proc_list.txt */
    FILE *pl = fopen("pieces/os/proc_list.txt", "w");
    if (pl) fclose(pl);
    log_pid(getpid(), "orchestrator");

    /* Clear state files */
    FILE *fc = fopen("pieces/display/frame_changed.txt", "w");
    if (fc) fclose(fc);
    FILE *rp = fopen("pieces/display/renderer_pulse.txt", "w");
    if (rp) fclose(rp);
    FILE *hr = fopen("pieces/apps/player_app/history.txt", "w");
    if (hr) fclose(hr);
    FILE *kr = fopen("pieces/keyboard/history.txt", "w");
    if (kr) fclose(kr);
    FILE *relay = fopen("pieces/apps/player_app/interact_relay.txt", "w");
    if (relay) fclose(relay);
    FILE *qf = fopen("pieces/system/quit_flag.txt", "w");
    if (qf) fclose(qf);
    FILE *wsc = fopen("pieces/display/wsr_screen_changed.txt", "w");
    if (wsc) fclose(wsc);

    setenv("PRISC_PROJECT_ROOT", cwd, 1);
    setenv("PRISC_PROJECT_ID", "wsr-pal", 1);

    fprintf(stderr, "[Orchestrator] Launching services...\n");
    launch("./system/renderer", NULL, NULL);
    launch("./system/keyboard_input", NULL, NULL);

    /* PAL layout: use PAL_LAYOUT env or default */
    const char *pal_layout = getenv("PAL_LAYOUT");
    if (pal_layout && pal_layout[0]) {
        fprintf(stderr, "[Orchestrator] Using PAL layout: %s\n", pal_layout);
        launch("./system/chtpm_parser_pal", pal_layout, NULL);
    } else {
        fprintf(stderr, "[Orchestrator] Using default: pieces/chtpm/layouts/wsr_main_menu.chtpm\n");
        launch("./system/chtpm_parser_pal", "pieces/chtpm/layouts/wsr_main_menu.chtpm", NULL);
    }

    /* Best effort: chtpm_rgb_render */
    fprintf(stderr, "[Orchestrator] Launching chtpm_rgb_render (best effort)...\n");
    launch("./system/chtpm_rgb_render", NULL, NULL);

    /* Best effort: gl_mirror (skip if NO_GL env set) */
    if (!getenv("NO_GL")) {
        fprintf(stderr, "[Orchestrator] Launching gl_mirror (best effort)...\n");
        launch("./system/gl_mirror", NULL, NULL);
    } else {
        fprintf(stderr, "[Orchestrator] Skipping gl_mirror (NO_GL set)\n");
    }

    fprintf(stderr, "[Orchestrator] Ready. Press Ctrl+C to exit.\n");

    while (!should_exit) {
        sleep(1);
        int status;
        pid_t dead;
        while ((dead = waitpid(-1, &status, WNOHANG)) > 0) {
            fprintf(stderr, "[Orchestrator] Child %d exited\n", dead);
        }
    }

    fprintf(stderr, "[Orchestrator] Exit.\n");
    return 0;
}
