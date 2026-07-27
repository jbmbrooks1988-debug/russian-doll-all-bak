/* orchestrator.c - background service launcher + 3-layer cascading kill
 * for muchi-pals (mass-refactor 2026-07-26, ported from 101.mutaclsym's
 * system/orchestrator.c, adapted for this project's per-session model).
 *
 * ADAPTATION FROM MUTACLSYM (deliberate, not a straight copy):
 *
 * 1. BACKGROUND-LAUNCHER, NOT THE FOREGROUND PROCESS. mutaclsym's
 *    button.sh does `exec system/orchestrator`, making orchestrator
 *    itself the foreground process attached to the terminal, and its
 *    main() loop only exits on a caught SIGINT/SIGTERM - it never
 *    watches quit_flag.txt. That works for mutaclsym because it has no
 *    concept of "return control to the shell after 'q'" separate from
 *    Ctrl+C. muchi-pals' button.sh already has a real, working,
 *    documented exit UX where `system/keyboard_input` runs as the
 *    foreground command and returning from it (on 'q') lets the script
 *    finish and hand the terminal back cleanly - a strictly better UX
 *    than requiring Ctrl+C. To keep that: button.sh launches this
 *    orchestrator in the BACKGROUND (before keyboard_input, which stays
 *    foreground and unchanged), and this file's own main() loop watches
 *    pieces/system/quit_flag.txt in addition to signals, self-triggering
 *    the same cascade-kill the instant keyboard_input writes it on 'q' -
 *    not just on Ctrl+C.
 *
 * 2. NO COMPILE-ON-LAUNCH. mutaclsym's orchestrator recompiles every
 *    binary via bare `gcc -o dst src` on every start. muchi-pals'
 *    scripts/build.sh already does real per-platform detection (X11 on
 *    Linux vs XQuartz on Mac vs Win32/GDI on Windows, FreeType for
 *    emoji_gen_atlas, -municode wmain on Windows) that would be wrong to
 *    re-derive here in miniature - button.sh's own `compile` action is
 *    still the one true build path, unchanged by this file.
 *
 * 3. SESSION-SCOPED KILL. Runs with cwd = the caller's session directory
 *    (button.sh's per-invocation throwaway dir under pieces/sessions/) -
 *    every relative path below (system/renderer, pieces/os/proc_list.txt,
 *    pieces/system/quit_flag.txt, ...) resolves through THAT session's
 *    own real dirs and symlinks back to the shared project root, exactly
 *    like every other process already launched by button.sh. proc_list.txt
 *    therefore only ever tracks THIS session's own children - never another
 *    concurrent session's - and the final kill_all.sh sweep is invoked
 *    with this session's own directory as an explicit argument so it can
 *    scope its pkill matching by /proc/pid/cwd instead of by command
 *    substring (see pieces/os/kill_all.sh's own header for why a bare
 *    substring match would be a real, documented, previously-fixed
 *    cross-session kill bug in this specific multi-session project).
 *
 * Everything else (fork/exec only, no system(); 3-layer cascading kill;
 * file-backed PID tracking under flock) follows Bible §3 / the family
 * pal-standards the same way mutaclsym's own orchestrator does.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>

static volatile int should_exit = 0;
static volatile int shutdown_done = 0;

/* === LAYER 1: Process Group Kill (fast path) === */

static void kill_process_group(void) {
    kill(0, SIGTERM);
    usleep(100000);
}

/* === LAYER 2: File-Backed PID Tracking (session-scoped proc_list.txt) === */

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
    while (fgets(line, sizeof(line), f)) {
        int pid; char name[128];
        if (sscanf(line, "%d %127s", &pid, name) == 2) {
            kill(pid, SIGTERM);
        }
    }
    fclose(f);
    usleep(200000);
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
    f = fopen("pieces/os/proc_list.txt", "w");
    if (f) fclose(f);
}

/* === LAYER 3: kill_all.sh Final Sweep, scoped to THIS session's cwd === */

static void run_final_kill_sweep(void) {
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';

    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/bash", "bash", "pieces/os/kill_all.sh", cwd, NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

/* === SHARED SHUTDOWN (reached via signal OR quit_flag.txt poll) === */

static void do_shutdown(void) {
    if (shutdown_done) return;
    shutdown_done = 1;
    fprintf(stderr, "[Orchestrator] Shutting down...\n");
    kill_process_group();
    kill_all_tracked();
    run_final_kill_sweep();
    fprintf(stderr, "[Orchestrator] Cleanup complete.\n");
}

void handle_signal(int sig) {
    (void)sig;
    should_exit = 1;
    do_shutdown();
    _exit(0);
}

/* === PROCESS LAUNCH (fork/exec, Bible §3 compliant) === */

static pid_t launch(const char *path, const char *arg1) {
    pid_t pid = fork();
    if (pid == 0) {
        if (arg1)
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

/* === DIRECTORY SETUP (fork/exec mkdir, Bible §3 compliant) === */

static void ensure_directories(void) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/mkdir", "mkdir", "-p", "pieces/os", NULL);
        _exit(1);
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

static int quit_requested(void) {
    struct stat st;
    if (stat("pieces/system/quit_flag.txt", &st) != 0) return 0;
    return st.st_size > 0;
}

/* === MAIN === */

int main(void) {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    fprintf(stderr, "[Orchestrator] Starting muchi-pals session from %s\n", cwd);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    ensure_directories();

    FILE *pl = fopen("pieces/os/proc_list.txt", "w");
    if (pl) fclose(pl);
    log_pid(getpid(), "orchestrator");

    fprintf(stderr, "[Orchestrator] Launching services...\n");
    launch("./system/renderer", NULL);

    const char *pal_layout = getenv("PAL_LAYOUT");
    if (pal_layout && pal_layout[0]) {
        launch("./system/chtpm_parser_pal", pal_layout);
    }

    /* Best effort, same as mutaclsym: only launched if actually built -
     * muchi-pals doesn't currently ship these two, harmless no-op here. */
    {
        struct stat st;
        if (stat("./system/chtpm_rgb_render", &st) == 0)
            launch("./system/chtpm_rgb_render", NULL);
        if (!getenv("NO_GL") && stat("./system/gl_mirror", &st) == 0)
            launch("./system/gl_mirror", NULL);
    }

    fprintf(stderr, "[Orchestrator] Ready. Waiting for quit_flag.txt or signal.\n");

    while (!should_exit) {
        if (quit_requested()) {
            should_exit = 1;
            do_shutdown();
            break;
        }
        int status;
        pid_t dead;
        while ((dead = waitpid(-1, &status, WNOHANG)) > 0) {
            fprintf(stderr, "[Orchestrator] Child %d exited\n", dead);
        }
        usleep(200000);
    }

    fprintf(stderr, "[Orchestrator] Exit.\n");
    return 0;
}
