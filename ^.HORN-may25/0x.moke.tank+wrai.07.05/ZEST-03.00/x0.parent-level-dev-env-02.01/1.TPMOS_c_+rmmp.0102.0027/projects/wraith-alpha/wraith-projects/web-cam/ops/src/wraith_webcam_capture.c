#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LEN 1024

static volatile sig_atomic_t g_stop = 0;
static pid_t g_child_pid = -1;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static int run_cmd(const char *cmd) {
    int status = system(cmd);
    if (status < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

static void shell_quote(char *out, size_t out_sz, const char *in) {
    size_t oi = 0;
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!in) return;
    while (*in && oi + 1 < out_sz) {
        if (*in == '\'') {
            if (oi + 4 >= out_sz) break;
            out[oi++] = '\'';
            out[oi++] = '\\';
            out[oi++] = '\'';
            out[oi++] = '\'';
        } else {
            out[oi++] = *in;
        }
        in++;
    }
    out[oi] = '\0';
}

static pid_t read_pid_file(const char *path) {
    FILE *f = fopen(path, "r");
    pid_t pid = -1;
    if (!f) return -1;
    if (fscanf(f, "%d", &pid) != 1) {
        pid = -1;
    }
    fclose(f);
    return pid;
}

static void resolve_device(char *out, size_t out_sz, const char *requested) {
    struct stat st;
    int i;
    if (!out || out_sz == 0) return;
    snprintf(out, out_sz, "%s", requested && requested[0] ? requested : "/dev/video0");
    if (stat(out, &st) == 0) {
        return;
    }
    for (i = 0; i < 10; i++) {
        snprintf(out, out_sz, "/dev/video%d", i);
        if (stat(out, &st) == 0) {
            return;
        }
    }
    snprintf(out, out_sz, "%s", requested && requested[0] ? requested : "/dev/video0");
}

static void resolve_profile(char *out, size_t out_sz, const char *requested) {
    if (!out || out_sz == 0) return;
    if (requested && strcmp(requested, "debug") == 0) {
        snprintf(out, out_sz, "debug");
        return;
    }
    snprintf(out, out_sz, "fast");
}

static void path_join(char *out, size_t out_sz, const char *root, const char *rel) {
    snprintf(out, out_sz, "%s/%s", root && root[0] ? root : ".", rel);
}

static void write_status(const char *root, const char *state, const char *detail, long frame_epoch) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/webcam.status");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "state=%s\n", state ? state : "unknown");
    fprintf(f, "detail=%s\n", detail ? detail : "");
    fprintf(f, "frame_epoch=%ld\n", frame_epoch);
    fclose(f);
}

static void pulse_marker(const char *root, const char *msg) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/fs_watch.marker");
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "webcam %s %ld\n", msg ? msg : "tick", (long)time(NULL));
    fclose(f);
}

static void kill_pid_file(const char *path) {
    pid_t pid = read_pid_file(path);
    if (pid > 1) {
        kill(pid, SIGTERM);
        usleep(300000);
        if (kill(pid, 0) == 0) {
            kill(pid, SIGKILL);
        }
    }
    remove(path);
}

static void kill_residual_ffmpeg(const char *root) {
    char current_frame[MAX_PATH_LEN];
    char quoted_frame[MAX_PATH_LEN * 2];
    char cmd[MAX_PATH_LEN * 4];

    path_join(current_frame, sizeof(current_frame), root, "session/current_frame.png");
    shell_quote(quoted_frame, sizeof(quoted_frame), current_frame);

    snprintf(cmd, sizeof(cmd),
        "pkill -TERM -f -- '%s' >/dev/null 2>&1; "
        "sleep 1; "
        "pkill -KILL -f -- '%s' >/dev/null 2>&1",
        quoted_frame, quoted_frame);
    run_cmd(cmd);
}

static void kill_previous(const char *root) {
    char daemon_pid[MAX_PATH_LEN];
    char ffmpeg_pid[MAX_PATH_LEN];
    path_join(daemon_pid, sizeof(daemon_pid), root, "session/webcam.pid");
    path_join(ffmpeg_pid, sizeof(ffmpeg_pid), root, "session/webcam.ffmpeg.pid");
    kill_pid_file(ffmpeg_pid);
    kill_pid_file(daemon_pid);
    kill_residual_ffmpeg(root);
}

static void ensure_dirs(const char *root) {
    char cmd[MAX_PATH_LEN * 2];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/session'", root);
    run_cmd(cmd);
}

static void cleanup_child(void) {
    if (g_child_pid > 1) {
        kill(g_child_pid, SIGTERM);
        usleep(150000);
        kill(g_child_pid, SIGKILL);
        waitpid(g_child_pid, NULL, 0);
        g_child_pid = -1;
    }
}

static int daemon_main(const char *root, const char *device, const char *profile) {
    char current_frame[MAX_PATH_LEN];
    char daemon_pid_path[MAX_PATH_LEN];
    char ffmpeg_pid_path[MAX_PATH_LEN];
    char ffmpeg_log[MAX_PATH_LEN];
    char resolved_device[64];
    char resolved_profile[16];
    struct stat st;
    time_t last_mtime = 0;
    FILE *pidf;

    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);

    resolve_device(resolved_device, sizeof(resolved_device), device);
    resolve_profile(resolved_profile, sizeof(resolved_profile), profile);

    if (stat(resolved_device, &st) != 0) {
        write_status(root, "missing_device", resolved_device, 0);
        pulse_marker(root, "missing_device");
        return 1;
    }

    path_join(current_frame, sizeof(current_frame), root, "session/current_frame.png");
    path_join(daemon_pid_path, sizeof(daemon_pid_path), root, "session/webcam.pid");
    path_join(ffmpeg_pid_path, sizeof(ffmpeg_pid_path), root, "session/webcam.ffmpeg.pid");
    path_join(ffmpeg_log, sizeof(ffmpeg_log), root, "session/webcam.ffmpeg.log");
    remove(ffmpeg_log);
    pidf = fopen(daemon_pid_path, "w");
    if (pidf) {
        fprintf(pidf, "%d\n", getpid());
        fclose(pidf);
    }

    g_child_pid = fork();
    if (g_child_pid == 0) {
        freopen(ffmpeg_log, "w", stdout);
        freopen(ffmpeg_log, "a", stderr);
        if (strcmp(resolved_profile, "debug") == 0) {
            execlp("ffmpeg", "ffmpeg",
                "-loglevel", "error",
                "-nostdin",
                "-y",
                "-f", "video4linux2",
                "-framerate", "2",
                "-video_size", "640x480",
                "-i", resolved_device,
                "-vf", "fps=1,scale=320:240",
                "-update", "1",
                current_frame,
                (char *)NULL);
        }
        execlp("ffmpeg", "ffmpeg",
            "-loglevel", "error",
            "-nostdin",
            "-y",
            "-f", "video4linux2",
            "-framerate", "8",
            "-video_size", "320x240",
            "-i", resolved_device,
            "-vf", "fps=4,scale=240:180",
            "-update", "1",
            current_frame,
            (char *)NULL);
        _exit(127);
    }
    if (g_child_pid < 0) {
        write_status(root, "error", "ffmpeg_fork_failed", 0);
        remove(daemon_pid_path);
        return 1;
    }
    pidf = fopen(ffmpeg_pid_path, "w");
    if (pidf) {
        fprintf(pidf, "%d\n", g_child_pid);
        fclose(pidf);
    }

    write_status(root, "streaming", resolved_profile, 0);
    pulse_marker(root, "started");

    while (!g_stop) {
        pid_t child_state = waitpid(g_child_pid, NULL, WNOHANG);
        if (child_state == g_child_pid) {
            g_child_pid = -1;
            write_status(root, "stopped", "ffmpeg_exited", 0);
            break;
        }
        if (stat(current_frame, &st) == 0) {
            if (st.st_mtime != last_mtime) {
                last_mtime = st.st_mtime;
                write_status(root, "streaming", resolved_profile, (long)last_mtime);
                pulse_marker(root, "frame");
            }
        }
        usleep(250000);
    }

    cleanup_child();
    write_status(root, "stopped", resolved_profile, (long)last_mtime);
    pulse_marker(root, "stopped");
    remove(daemon_pid_path);
    remove(ffmpeg_pid_path);
    return 0;
}

int main(int argc, char **argv) {
    const char *mode;
    const char *root;
    const char *device = "/dev/video0";
    const char *profile = "fast";

    if (argc < 3) {
        fprintf(stderr, "usage: wraith_webcam_capture <--start|--stop> <project_root> [device]\n");
        return 1;
    }
    mode = argv[1];
    root = argv[2];
    if (argc > 3 && argv[3][0]) {
        device = argv[3];
    }
    if (argc > 4 && argv[4][0]) {
        profile = argv[4];
    }

    ensure_dirs(root);

    if (strcmp(mode, "--stop") == 0) {
        kill_previous(root);
        write_status(root, "stopped", "stop_requested", 0);
        pulse_marker(root, "stop_requested");
        return 0;
    }
    if (strcmp(mode, "--start") != 0) {
        return 1;
    }

    kill_previous(root);

    {
        pid_t pid = fork();
        if (pid == 0) {
            if (setsid() < 0) {
                _exit(1);
            }
            freopen("/dev/null", "r", stdin);
            freopen("/dev/null", "a", stdout);
            freopen("/dev/null", "a", stderr);
            _exit(daemon_main(root, device, profile));
        }
        if (pid < 0) {
            write_status(root, "error", profile, 0);
            return 1;
        }
    }

    write_status(root, "starting", profile, 0);
    pulse_marker(root, "starting");
    return 0;
}
