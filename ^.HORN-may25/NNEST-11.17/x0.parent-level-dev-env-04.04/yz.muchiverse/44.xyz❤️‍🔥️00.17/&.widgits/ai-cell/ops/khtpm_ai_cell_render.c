/* khtpm_ai_cell_render.c — the taskbar's cell 14 ("ai") real window.
 * Design doc: au11-hq/AI-CELL-GUI-DESIGN.md (read that FIRST — this
 * file implements its §3/§4/§5/§7/§9 decisions, doesn't re-derive
 * them).
 *
 * v1 scope (direct instruction, 2026-08-12): raw Ollama HTTP as the
 * MAIN backend (10.0.0.144:11434, see #.Z.HUMAN_LLM/.MAC-ACCESS.txt),
 * agent-45 relay kept as a documented-but-not-yet-wired "legacy hook"
 * (see g_backend_mode below) rather than built out fully this pass.
 *
 * REAL FIX 2026-08-12, direct instruction ("lets make sure we have
 * transcript scrolling so we can audit history... i want a way i can
 * read previous historic chat with sidebar option or something (and
 * delete it if i want)"): added real disk-persisted chat sessions
 * (sidebar list, load/delete) and real transcript scrolling. Scroll
 * convention is PORTED, not invented - user's own correction: "it
 * just uses a [] up and [] down nav button to scroll view up or down
 * its nothing spectacular" - matches wraith-alpha's own fs scroll
 * pattern (1.TPMOS_c_+rmmp.0103.0001/.../wraith_project_input.c):
 * scroll_offset + a fixed visible-window size + nav-badge-numbered up/
 * down buttons, NOT tpmos's own separate joystick/GL-thumb scrollbar
 * (gl_desktop.c) - that one doesn't fit this file's plain X11/nav
 * shape at all, deliberately not used.
 *
 * Because the sidebar now grows with saved sessions, nav is a real
 * dynamic array (g_nav[]) built fresh each frame - same shape as db-hq/
 * events-hq's own Elem->nav_index assignment, adapted to this file's
 * flat (non-tag-tree) layout.
 *
 * Reuses every proven house pattern rather than reinventing:
 * managed window + _MOTIF_WM_HINTS (NOT override_redirect - the real
 * keyboard-focus fix, !.HOUSE_STDS.md #21), RGB compose->present
 * (XGetImage->XPutImage, same as db-hq/events-hq/entities), wraith-
 * alpha nav (bracket badges, digit-jump, HOUSE_STDS #22), Xft text
 * (no GL, HOUSE_STDS #24's non-fatal XSetErrorHandler).
 *
 * Layout is hand-computed x/y/w/h (khtpm_css_parser.c has no flex/grid
 * - same constraint db-hq/events-hq already work within; this file
 * doesn't use the CSS engine at all yet, see AI-CELL-GUI-DESIGN.md §9).
 *
 * Usage: khtpm_ai_cell_render.+x <house_root>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define PATH_BUF 4096
#define WIN_W 1000
#define WIN_H 680
#define SIDEBAR_W 240
#define TOPBAR_H 44
#define COMPOSER_H 64
#define COMPOSER_MAX_LINES 6 /* grows the composer box as you type wrapped lines, up to this many visible at once before it auto-scrolls (keeps the cursor's line visible) instead of running text off the edge - direct report 2026-08-13: "text input i want it to wrap new line and user input can scroll up instead of dissapearing off the side of the screen" */
#define CHROME_H 28
#define MAX_MSGS 512
#define MSG_LEN 8192
#define MAX_SESSIONS 64
#define MAX_NAV 96
#define INPUT_BUF_LEN 4096
#define LINE_H 19 /* 18 + ~5% (readability - user: lines were "completely stacked") */
#define MAX_FLAT_LINES 4096

/* Audit artifacts live in the house tree (xyzfs, not /tmp) so a human can
 * audit every run: frames, receipts, payloads, responses, tool outputs,
 * runtime log. Same pattern as board-viewer writing receipts into its own
 * pieces/ dir. */
#define AUDIT_DIR_REL "&.widgits/ai-cell/pieces/audit"
#define AUDIT_EMOJI_REL "&.widgits/ai-cell/pieces/registry/emoji_assets"

static char g_house_root[PATH_BUF];
static char g_sessions_root[PATH_BUF];
static char g_audit_dir[PATH_BUF];
static char g_pid_path[PATH_BUF];
static char g_emoji_dir[PATH_BUF];
static int g_running = 1; /* global so the real nav-indexed close button (NAV_CLOSE) can set it from activate_focused() */
static unsigned g_frame = 0; /* redraw counter - drives the thinking animation */

/* ---------- non-fatal X error handler (HOUSE_STDS #24) ---------- */
static int nonfatal_x_error(Display *dpy, XErrorEvent *ev) {
    (void)dpy;
    fprintf(stderr, "ai-cell: non-fatal X error, code=%d request=%d\n", ev->error_code, ev->request_code);
    return 0;
}

/* ---------- transcript (chat message log) ---------- */
typedef struct { int is_user; char text[MSG_LEN]; } ChatMsg;
static ChatMsg g_msgs[MAX_MSGS];
static int g_n_msgs = 0;

/* ---------- session persistence (real disk history, direct
 * instruction: "i want a way i can read previous historic chat with
 * sidebar option or something (and delete it if i want) so i can
 * audit wether u got it working meaningfully yet") ----------
 * One dir per session under &.widgits/ai-cell/sessions/<epoch>/,
 * one flat transcript.txt, one line per message:
 *   U|<text with real \n escaped as literal \n, \ escaped as \\>
 *   A|<text, same escaping>
 * Simple enough to hand-parse (matches this house's own "don't pull
 * in a library for one format" convention), auditable as plain text
 * (direct ask - "audit... history"), not a binary/opaque format. */
static char g_session_dir[PATH_BUF] = "";

typedef struct { char dir[PATH_BUF]; char label[80]; } SessionEntry;
static SessionEntry g_sessions[MAX_SESSIONS];
static int g_n_sessions = 0;

static void escape_line(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 2 < outsz; i++) {
        if (in[i] == '\\') { out[o++] = '\\'; out[o++] = '\\'; }
        else if (in[i] == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else out[o++] = in[i];
    }
    out[o] = '\0';
}

static void unescape_line(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < outsz; i++) {
        if (in[i] == '\\' && in[i + 1] == 'n') { out[o++] = '\n'; i++; }
        else if (in[i] == '\\' && in[i + 1] == '\\') { out[o++] = '\\'; i++; }
        else out[o++] = in[i];
    }
    out[o] = '\0';
}

static void persist_msg(int is_user, const char *text) {
    if (!g_session_dir[0]) return;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/transcript.txt", g_session_dir);
    FILE *f = fopen(path, "a");
    if (!f) return;
    char esc[MSG_LEN * 2];
    escape_line(text, esc, sizeof(esc));
    fprintf(f, "%c|%s\n", is_user ? 'U' : 'A', esc);
    fclose(f);
}

static void add_msg(int is_user, const char *text) {
    if (g_n_msgs >= MAX_MSGS) {
        memmove(&g_msgs[0], &g_msgs[1], sizeof(ChatMsg) * (MAX_MSGS - 1));
        g_n_msgs = MAX_MSGS - 1;
    }
    g_msgs[g_n_msgs].is_user = is_user;
    snprintf(g_msgs[g_n_msgs].text, sizeof(g_msgs[g_n_msgs].text), "%s", text);
    g_n_msgs++;
}

/* like add_msg, but also writes to disk - use for real conversation
 * turns; add_msg alone (no persist) is for viewing a LOADED read-only
 * historic session without re-appending its own lines back to itself. */
static void add_and_persist(int is_user, const char *text) {
    add_msg(is_user, text);
    persist_msg(is_user, text);
}

static void start_new_session(void) {
    g_n_msgs = 0;
    time_t now = time(NULL);
    snprintf(g_session_dir, sizeof(g_session_dir), "%s/%ld", g_sessions_root, (long)now);
    mkdir(g_sessions_root, 0755);
    mkdir(g_session_dir, 0755);
}

/* Loads a past session's transcript.txt into g_msgs for VIEWING
 * (read-only - sending a new message from a loaded historic session
 * starts appending to THAT session's own file again, matching normal
 * "continue this chat" expectations, not a separate read-only mode). */
static void load_session(const char *dir) {
    snprintf(g_session_dir, sizeof(g_session_dir), "%s", dir);
    g_n_msgs = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/transcript.txt", dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MSG_LEN * 2];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (n < 2 || line[1] != '|') continue;
        int is_user = (line[0] == 'U');
        char text[MSG_LEN];
        unescape_line(line + 2, text, sizeof(text));
        add_msg(is_user, text);
    }
    fclose(f);
}

static void delete_session(const char *dir) {
    char cmd[PATH_BUF + 32];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    int rc = system(cmd);
    (void)rc;
}

static int session_cmp_desc(const void *a, const void *b) {
    const SessionEntry *sa = a, *sb = b;
    return strcmp(sb->dir, sa->dir); /* dir name = epoch seconds, so string compare = numeric order */
}

/* Rescans &.widgits/ai-cell/sessions/ - call after create/delete so
 * the sidebar list is never stale. Label = timestamp formatted
 * human-readable, plus a short snippet of the first user message if
 * one exists (so the sidebar list is actually useful for audit, not
 * just a wall of timestamps). */
/* Forward declarations for model functions */
static void load_model_choice(void);
static void save_model_choice(void);
static void cycle_model(void);

static void refresh_sessions(void) {
    g_n_sessions = 0;
    DIR *d = opendir(g_sessions_root);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && g_n_sessions < MAX_SESSIONS) {
        if (e->d_name[0] == '.') continue;
        char full[PATH_BUF];
        snprintf(full, sizeof(full), "%s/%s", g_sessions_root, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        SessionEntry *se = &g_sessions[g_n_sessions];
        snprintf(se->dir, sizeof(se->dir), "%s", full);
        time_t epoch = (time_t)atol(e->d_name);
        struct tm *tmv = localtime(&epoch);
        char tsbuf[32] = "";
        if (tmv) strftime(tsbuf, sizeof(tsbuf), "%m-%d %H:%M", tmv);
        char snippet[48] = "";
        char tpath[PATH_BUF];
        snprintf(tpath, sizeof(tpath), "%s/transcript.txt", full);
        FILE *tf = fopen(tpath, "r");
        if (tf) {
            char line[256];
            while (fgets(line, sizeof(line), tf)) {
                if (line[0] == 'U' && line[1] == '|') {
                    char raw[256];
                    snprintf(raw, sizeof(raw), "%s", line + 2);
                    raw[strcspn(raw, "\r\n")] = '\0';
                    char un[64];
                    unescape_line(raw, un, sizeof(un));
                    snprintf(snippet, sizeof(snippet), "%s", un);
                    break;
                }
            }
            fclose(tf);
        }
        if (snippet[0]) snprintf(se->label, sizeof(se->label), "%s %s", tsbuf, snippet);
        else snprintf(se->label, sizeof(se->label), "%s (empty)", tsbuf);
        g_n_sessions++;
    }
    closedir(d);
    qsort(g_sessions, (size_t)g_n_sessions, sizeof(SessionEntry), session_cmp_desc);
}

/* ---------- backend: raw Ollama HTTP (MAIN, per direct instruction) ----------
 * "since u said raw use for ollama is better, lets just use that as main
 * but we can have legacy hook option or w/e" - agent-45 relay (see
 * AI-CELL-GUI-DESIGN.md §6/§7) is the documented future secondary
 * backend, not built out this pass; g_backend_mode exists so adding it
 * later is a switch-case addition, not a rewrite. */
/* BACKEND_HARNECIENT added Phase 2 2026-08-13: for non-tooled models
 * (270m/1b/stable-code-3b/llama2), send_to_ollama() prepends that
 * model's persona (pieces/registry/personas/<slug>.txt) to the prompt
 * text and NEVER includes a "tools" field in the payload - matches
 * every other model in this house that lacks native tool support
 * (see AI-CELL-GUI-DESIGN.md / this file's Phase 2 roadmap entry,
 * 13.AUG.13-HAI-2do.txt). BACKEND_OLLAMA_RAW remains for the one
 * NATIVE-tools model (llama3-groq-tool-use:8b) - no persona injection,
 * unchanged behavior. */
typedef enum { BACKEND_OLLAMA_RAW = 0, BACKEND_AGENT45_LEGACY = 1, BACKEND_HARNECIENT = 2 } BackendMode;
static BackendMode g_backend_mode = BACKEND_OLLAMA_RAW;
static char g_model_name[128] = "stable-code:latest";

/* Model whitelist: (model_name, backend_mode) */
typedef struct { const char *name; BackendMode mode; } ModelEntry;
static const ModelEntry g_models[] = {
    { "stable-code:latest", BACKEND_HARNECIENT },  /* 3B, Harnecient */
    { "gemma3:1b", BACKEND_HARNECIENT },           /* Harnecient */
    { "gemma3:270m", BACKEND_HARNECIENT },         /* Harnecient */
    { "llama3-groq-tool-use:8b", BACKEND_OLLAMA_RAW }, /* NATIVE tools */
    { "llama2:latest", BACKEND_HARNECIENT }        /* Harnecient */
};
static int g_model_idx = 0;  /* current model index in g_models */
static const int g_n_models = sizeof(g_models) / sizeof(g_models[0]);

static const char *g_ollama_host = "10.0.0.144:11434";

static int g_pending = 0;          /* 1 while a curl child is in flight */
static pid_t g_pending_pid = -1;
static char g_pending_outfile[PATH_BUF];
static int g_pending_is_tool = 0;  /* 1 while the in-flight child job is a tool (raw text output), not Ollama JSON */

/* Fires the async request: writes a JSON prompt file (so we don't fight
 * shell-quoting on arbitrary user text), shells out to curl in a
 * detached child, writes curl's raw response body to g_pending_outfile.
 * Polled non-blocking from the main loop (see check_pending() below) -
 * same "fire, don't block the render loop" shape as agent-45's own
 * ai_state=THINKING polling convention (state.txt, confirmed live this
 * session), not reinvented. */
static void escape_json_string(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 2 < outsz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c >= 32) { out[o++] = (char)c; }
    }
    out[o] = '\0';
}

/* Loads pieces/registry/personas/<model-slug>.txt (model name with
 * ':' replaced by '-', matching T2.1's file naming) into out - caller
 * must size out generously (a persona file is expected to be short,
 * a few sentences). Returns 0 and leaves out empty if no persona file
 * exists for this model (not an error - BACKEND_OLLAMA_RAW models
 * like the native-tools 8b legitimately may or may not want one). */
static int load_persona(const char *model_name, char *out, size_t outsz) {
    out[0] = '\0';
    char slug[128];
    size_t si = 0;
    for (const char *p = model_name; *p && si + 1 < sizeof(slug); p++) {
        slug[si++] = (*p == ':') ? '-' : *p;
    }
    slug[si] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/&.widgits/ai-cell/pieces/registry/personas/%s.txt", g_house_root, slug);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, outsz - 1, f);
    out[n] = '\0';
    fclose(f);
    return n > 0;
}

static void send_to_ollama(const char *prompt) {
    if (g_pending) return; /* one in flight at a time */

    /* Harnecient path: prepend the model's persona to the prompt text
     * itself (not a separate "system" JSON field) - keeps the payload
     * shape identical to the RAW path (single "prompt" string, never a
     * "tools" field), per Phase 2 T2.1/T2.3 (13.AUG.13-HAI-2do.txt). */
    char full_prompt[MSG_LEN + 2048];
    const char *prompt_to_send = prompt;
    if (g_backend_mode == BACKEND_HARNECIENT) {
        char persona[2048];
        if (load_persona(g_model_name, persona, sizeof(persona))) {
            snprintf(full_prompt, sizeof(full_prompt), "%s\n\n%s", persona, prompt);
            prompt_to_send = full_prompt;
        }
    }

    char esc[MSG_LEN * 2 + 4096];
    escape_json_string(prompt_to_send, esc, sizeof(esc));

    char payload_path[PATH_BUF];
    snprintf(payload_path, sizeof(payload_path), "%s/payload-%d.json", g_audit_dir, (int)getpid());
    FILE *pf = fopen(payload_path, "w");
    if (!pf) return;
    fprintf(pf, "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false}", g_model_name, esc);
    fclose(pf);

    snprintf(g_pending_outfile, sizeof(g_pending_outfile), "%s/response-%d.json", g_audit_dir, (int)getpid());
    unlink(g_pending_outfile);

    pid_t pid = fork();
    if (pid == 0) {
        char url[256];
        snprintf(url, sizeof(url), "http://%s/api/generate", g_ollama_host);
        int fd = open(g_pending_outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { dup2(fd, 1); close(fd); }
        char data_arg[PATH_BUF + 2];
        snprintf(data_arg, sizeof(data_arg), "@%s", payload_path);
        execlp("curl", "curl", "-s", "-m", "60", "-X", "POST", url,
               "-H", "Content-Type: application/json", "-d", data_arg,
               (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        g_pending = 1;
        g_pending_pid = pid;
    }
}

/* Pulls just the "response" field's string value out of Ollama's JSON
 * reply with a small hand-rolled scan (same "don't pull in a JSON
 * library for one field" convention already used elsewhere in this
 * house for PDL-adjacent parsing) - good enough for a single top-level
 * string field, not a general JSON parser. */
/* REAL BUG found and fixed 2026-08-13 (Stage A pilot #1,
 * EVENTS-PAL-BUILDOUT-PLAN.md §6): this parser handled \n and \t but
 * had no \uXXXX case - for a `>` (Ollama's own standard JSON
 * escaping of '>', and `&` for '&') it fell into the generic
 * `else out[o++] = *p;` branch, which after the backslash-skip just
 * emitted the literal 'u' character and let "003e"/"0026" fall
 * through as ordinary text - silently corrupting EVERY model
 * response containing '>', '&', or any other \u-escaped character
 * into garbage like "u003e" instead of '>'. Confirmed live: two
 * INDEPENDENT models (stable-code:latest, gemma3:1b) both produced
 * this exact corruption pattern on the same delegated task, which is
 * what correctly flagged this as an ai-cell bug, not a model
 * reliability issue - a real cross-model artifact means the common
 * factor (this parser) is the actual culprit. Decodes the BMP range
 * (4 hex digits -> UTF-8, 1-3 bytes) - surrogate pairs (rare, astral
 * codepoints) are not handled, out of scope for this fix. */
static void extract_response_field(const char *json, char *out, size_t outsz) {
    const char *key = "\"response\":\"";
    const char *p = strstr(json, key);
    out[0] = '\0';
    if (!p) return;
    p += strlen(key);
    size_t o = 0;
    while (*p && *p != '"' && o + 1 < outsz) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n') { out[o++] = '\n'; }
            else if (*p == 't') { out[o++] = '\t'; }
            else if (*p == 'u' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2]) &&
                     isxdigit((unsigned char)p[3]) && isxdigit((unsigned char)p[4])) {
                char hex[5] = { p[1], p[2], p[3], p[4], '\0' };
                unsigned int cp = (unsigned int)strtoul(hex, NULL, 16);
                p += 4;
                if (cp < 0x80) {
                    if (o + 1 < outsz) out[o++] = (char)cp;
                } else if (cp < 0x800) {
                    if (o + 2 < outsz) {
                        out[o++] = (char)(0xC0 | (cp >> 6));
                        out[o++] = (char)(0x80 | (cp & 0x3F));
                    }
                } else {
                    if (o + 3 < outsz) {
                        out[o++] = (char)(0xE0 | (cp >> 12));
                        out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        out[o++] = (char)(0x80 | (cp & 0x3F));
                    }
                }
            }
            else { out[o++] = *p; }
        } else {
            out[o++] = *p;
        }
        p++;
    }
    out[o] = '\0';
}

static int g_scroll_follow_bottom = 1; /* forward-declared use below */

static void check_pending(void) {
    if (!g_pending) return;
    int status;
    pid_t r = waitpid(g_pending_pid, &status, WNOHANG);
    if (r != g_pending_pid) return;
    g_pending = 0;

    if (g_pending_is_tool) {
        g_pending_is_tool = 0;
        FILE *f = fopen(g_pending_outfile, "r");
        if (!f) { add_and_persist(0, "[tool error: no output file]"); g_scroll_follow_bottom = 1; return; }
        char buf[MSG_LEN * 2];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        unlink(g_pending_outfile);
        add_and_persist(0, buf[0] ? buf : "[tool: no output]");
        g_scroll_follow_bottom = 1;
        return;
    }

    FILE *f = fopen(g_pending_outfile, "r");
    if (!f) { add_and_persist(0, "[error: curl produced no output]"); g_scroll_follow_bottom = 1; return; }
    char buf[MSG_LEN * 4];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    unlink(g_pending_outfile);

    char resp[MSG_LEN];
    extract_response_field(buf, resp, sizeof(resp));
    if (resp[0]) add_and_persist(0, resp);
    else add_and_persist(0, "[error: no 'response' field in Ollama reply - check model name / endpoint]");
    g_scroll_follow_bottom = 1;
}

/* ---------- REAL TOOLS ----------
 * Ported from agent-45's PROVEN mechanism (045.muchi-pal-agent/ops/
 * gemma_strategy.c + strategy_execute_a.c, read directly 2026-08-12),
 * not invented: the USER's message is parsed DETERMINISTICALLY for tool
 * keywords - the model never emits tool calls. Read-only tools
 * (list_dir/read_file/search_in_files) pre-execute like agent-45's
 * Strategy A; mutating tools (write_file/edit_file) and cmd_exec require
 * an explicit approve/deny through the nav, the same shape as agent-45's
 * own execute_tool/deny_tool user-approval pair (human in the loop for
 * anything that writes to disk or runs a shell command).
 *
 * Detection is deliberately conservative so natural-language CHAT ("write
 * a C function that...") still goes to the model: a tool only fires when
 * a path-looking token (contains '/' or '.') follows the keyword. */

#define TOOL_MAX_ARG 512

typedef struct {
    char name[32];           /* read_file|write_file|edit_file|search_in_files|list_dir|cmd_exec */
    char arg[TOOL_MAX_ARG];  /* file path / search query / shell command */
    char search[TOOL_MAX_ARG]; /* edit_file replace: the old text ("" = append mode) */
    char content[MSG_LEN];   /* text to write/append (write_file/edit_file) */
} PendingTool;

static PendingTool g_pending_tool;
static int g_tool_pending = 0; /* 1 = a mutating/exec tool awaits approve/deny in the nav */

static void to_lower_str(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static char *find_word(const char *text, const char *word) {
    size_t klen = strlen(word);
    for (char *p = (char *)text; *p; p++) {
        if (strncasecmp(p, word, klen) == 0) {
            char before = (p == text) ? ' ' : *(p - 1);
            char after = *(p + klen);
            if (!isalnum((unsigned char)before) && !isalnum((unsigned char)after)) return p;
        }
    }
    return NULL;
}

static int earliest_kw(const char *text, const char *kws[], int *klen_out) {
    int best = -1, best_len = 0;
    for (int i = 0; kws[i]; i++) {
        char *h = find_word(text, kws[i]);
        if (h && (best < 0 || (h - text) < best || ((h - text) == best && (int)strlen(kws[i]) > best_len))) {
            best = (int)(h - text);
            best_len = (int)strlen(kws[i]);
        }
    }
    *klen_out = best_len;
    return best;
}

static void trim_ws(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static void strip_outer_quotes(char *s) {
    size_t len = strlen(s);
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') ||
                     (s[0] == '\'' && s[len - 1] == '\''))) {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

static int next_token(char **cursor, char *out, size_t outsz) {
    char *p = *cursor;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    char *end = p;
    while (*end && !isspace((unsigned char)*end)) end++;
    size_t len = (size_t)(end - p);
    if (len >= outsz) len = outsz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    *cursor = end;
    return 1;
}

static int tok_is_pathish(const char *tok) {
    return tok[0] && (strchr(tok, '/') != NULL || strchr(tok, '.') != NULL);
}

/* last path-looking token after `start` ("" if none) - the file path in
 * "write the code to /tmp/foo.c" / "read file at ./x.txt" */
static int extract_path_arg(const char *start, char *out, size_t outsz) {
    char tok[256], found[256] = "";
    char *cursor = (char *)start;
    int hit = 0;
    while (next_token(&cursor, tok, sizeof(tok))) {
        if (tok_is_pathish(tok)) { snprintf(found, sizeof(found), "%s", tok); hit = 1; }
    }
    if (!hit) return 0;
    strip_outer_quotes(found);
    snprintf(out, outsz, "%s", found);
    return 1;
}

static const char *last_assistant_text(void) {
    for (int i = g_n_msgs - 1; i >= 0; i--)
        if (!g_msgs[i].is_user) return g_msgs[i].text;
    return "";
}

static int tool_requires_approval(const char *name) {
    return strcmp(name, "write_file") == 0 ||
           strcmp(name, "edit_file") == 0 ||
           strcmp(name, "cmd_exec") == 0;
}

/* detect_tool: 1 = real tool found (pt filled), 0 = ordinary chat.
 * Mirrors agent-45's gemma_strategy detect order: read -> write -> edit
 * -> search -> list -> exec. */
static int detect_tool(const char *msg, PendingTool *pt) {
    char lower[MSG_LEN];
    snprintf(lower, sizeof(lower), "%s", msg);
    to_lower_str(lower);
    PendingTool t;
    memset(&t, 0, sizeof(t));
    int off, klen;

    /* --- read_file: <read|open|cat|view|display> + pathish token --- */
    {
        const char *kws[] = {"read file", "open file", "cat file", "view file",
                             "read", "open", "cat", "view", "display", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        if (extract_path_arg(tail, t.arg, sizeof(t.arg))) {
            snprintf(t.name, sizeof(t.name), "read_file");
            *pt = t;
            return 1;
        }
    }

    /* --- write_file: <write|create|save> + pathish token; content =
     * text after "containing"/"that says" else the model's last answer
     * (the "write the code it just made to file" use case) --- */
    {
        const char *kws[] = {"write file", "create file", "save file",
                             "write", "create", "save", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        if (extract_path_arg(tail, t.arg, sizeof(t.arg))) {
            snprintf(t.name, sizeof(t.name), "write_file");
            char *bestc = NULL;
            int bestclen = 0;
            const char *cps[] = {"containing", "that says", NULL};
            for (int i = 0; cps[i]; i++) {
                char *h = find_word(lower, cps[i]);
                if (h && (!bestc || h < bestc)) { bestc = h; bestclen = (int)strlen(cps[i]); }
            }
            if (bestc) {
                char *c = (char *)msg + (bestc - lower) + bestclen;
                trim_ws(c);
                strip_outer_quotes(c);
                snprintf(t.content, sizeof(t.content), "%s", c);
            } else {
                const char *last = last_assistant_text();
                if (last[0]) snprintf(t.content, sizeof(t.content), "%s", last);
            }
            *pt = t;
            return 1;
        }
    }

    /* --- edit_file: <edit|modify|append> + pathish token.
     * "edit F replace OLD with NEW" -> replace; otherwise append (content =
     * text after "the line"/"the text", else last assistant answer). --- */
    {
        const char *kws[] = {"edit", "modify", "append", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        if (extract_path_arg(tail, t.arg, sizeof(t.arg))) {
            snprintf(t.name, sizeof(t.name), "edit_file");
            char *rkw = find_word(lower, "replace");
            char *wkw = find_word(lower, "with");
            if (rkw && wkw && wkw > rkw) {
                char *so = (char *)msg + (rkw - lower) + 7; /* after "replace" */
                size_t n = (size_t)(wkw - rkw) - 7;
                if (n > 0 && n < sizeof(t.search)) {
                    memcpy(t.search, so, n);
                    t.search[n] = '\0';
                    trim_ws(t.search);
                    strip_outer_quotes(t.search);
                }
                char *co = (char *)msg + (wkw - lower) + 4; /* after "with" */
                trim_ws(co);
                strip_outer_quotes(co);
                snprintf(t.content, sizeof(t.content), "%s", co);
            } else {
                const char *line_kws[] = {"the line", "the text", NULL};
                char *bc = NULL;
                int bclen = 0;
                for (int i = 0; line_kws[i]; i++) {
                    char *h = find_word(lower, line_kws[i]);
                    if (h && (!bc || h < bc)) { bc = h; bclen = (int)strlen(line_kws[i]); }
                }
                if (bc) {
                    char *c = (char *)msg + (bc - lower) + bclen;
                    trim_ws(c);
                    strip_outer_quotes(c);
                    snprintf(t.content, sizeof(t.content), "%s", c);
                } else {
                    const char *last = last_assistant_text();
                    if (last[0]) snprintf(t.content, sizeof(t.content), "%s", last);
                }
            }
            *pt = t;
            return 1;
        }
    }

    /* --- search_in_files: <search|grep> + query, optional " in <target>" --- */
    {
        const char *kws[] = {"search", "grep", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        trim_ws(tail);
        if (tail[0]) {
            char *in = find_word(lower, " in ");
            if (in && in > lower + off) {
                size_t qn = (size_t)(in - lower) - (size_t)(off + klen);
                if (qn > 0 && qn < sizeof(t.arg)) {
                    memcpy(t.arg, tail, qn);
                    t.arg[qn] = '\0';
                    trim_ws(t.arg);
                    strip_outer_quotes(t.arg);
                }
                char *tp = (char *)msg + (in - lower) + 4; /* after " in " */
                trim_ws(tp);
                extract_path_arg(tp, t.search, sizeof(t.search));
                if (t.arg[0]) {
                    snprintf(t.name, sizeof(t.name), "search_in_files");
                    *pt = t;
                    return 1;
                }
            } else {
                strip_outer_quotes(tail);
                snprintf(t.arg, sizeof(t.arg), "%s", tail);
                snprintf(t.name, sizeof(t.name), "search_in_files");
                *pt = t;
                return 1;
            }
        }
    }

    /* --- list_dir: <list|dir|show> + optional pathish token (default house root) --- */
    {
        const char *kws[] = {"list", "show", "dir", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        snprintf(t.name, sizeof(t.name), "list_dir");
        if (!extract_path_arg(tail, t.arg, sizeof(t.arg))) t.arg[0] = '\0'; /* default */
        *pt = t;
        return 1;
    }

    /* --- cmd_exec: <run|execute|command|cmd|exec> + command text --- */
    {
        const char *kws[] = {"run", "execute", "command", "cmd", "exec", NULL};
        off = earliest_kw(lower, kws, &klen);
    }
    if (off >= 0) {
        char *tail = (char *)msg + off + klen;
        trim_ws(tail);
        if (tail[0]) {
            snprintf(t.name, sizeof(t.name), "cmd_exec");
            snprintf(t.arg, sizeof(t.arg), "%s", tail);
            *pt = t;
            return 1;
        }
    }

    return 0;
}

/* ---- real tool execution (runs inside the forked child, writes raw text
 * to the outfile; check_pending surfaces it as an assistant message) ---- */
static void tool_list_dir(const char *path, char *out, size_t outsz) {
    char resolved[PATH_BUF];
    if (path[0] == '/') snprintf(resolved, sizeof(resolved), "%s", path);
    else snprintf(resolved, sizeof(resolved), "%s/%s", g_house_root, path);
    DIR *d = opendir(resolved);
    if (!d) { snprintf(out, outsz, "[list_dir] cannot open: %s", resolved); return; }
    char buf[MSG_LEN / 2] = "";
    snprintf(buf, sizeof(buf), "[list_dir] %s:\n", resolved);
    struct dirent *e;
    while ((e = readdir(d)) && strlen(buf) < sizeof(buf) - 512) {
        char line[300];
        snprintf(line, sizeof(line), "  %s%s\n", e->d_name, (e->d_type == DT_DIR) ? "/" : "");
        if (strlen(buf) + strlen(line) >= sizeof(buf)) {
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "  ... (truncated)");
            break;
        }
        strncat(buf, line, sizeof(buf) - strlen(buf) - 1);
    }
    closedir(d);
    snprintf(out, outsz, "%s", buf);
}

static void tool_read_file(const char *path, char *out, size_t outsz) {
    char resolved[PATH_BUF];
    if (path[0] == '/') snprintf(resolved, sizeof(resolved), "%s", path);
    else snprintf(resolved, sizeof(resolved), "%s/%s", g_house_root, path);
    FILE *f = fopen(resolved, "rb");
    if (!f) { snprintf(out, outsz, "[read_file] cannot open: %s", resolved); return; }
    char buf[MSG_LEN / 2];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    snprintf(out, outsz, "[read_file] %s:\n%s", resolved, buf);
}

static void tool_write_file(const PendingTool *pt, char *out, size_t outsz) {
    char resolved[PATH_BUF];
    if (pt->arg[0] == '/') snprintf(resolved, sizeof(resolved), "%s", pt->arg);
    else snprintf(resolved, sizeof(resolved), "%s/%s", g_house_root, pt->arg);
    if (!pt->content[0]) { snprintf(out, outsz, "[write_file] nothing to write (no content given, no prior ai answer)"); return; }
    FILE *f = fopen(resolved, "wb");
    if (!f) { snprintf(out, outsz, "[write_file] cannot create: %s", resolved); return; }
    size_t n = fwrite(pt->content, 1, strlen(pt->content), f);
    fclose(f);
    snprintf(out, outsz, "[write_file] wrote %zu bytes to %s", n, resolved);
}

static void tool_edit_file(const PendingTool *pt, char *out, size_t outsz) {
    char resolved[PATH_BUF];
    if (pt->arg[0] == '/') snprintf(resolved, sizeof(resolved), "%s", pt->arg);
    else snprintf(resolved, sizeof(resolved), "%s/%s", g_house_root, pt->arg);
    if (pt->search[0]) {
        FILE *f = fopen(resolved, "rb");
        if (!f) { snprintf(out, outsz, "[edit_file] cannot open: %s", resolved); return; }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0 || sz > MSG_LEN * 2) {
            fclose(f);
            snprintf(out, outsz, "[edit_file] file too big to edit in place");
            return;
        }
        char *data = malloc((size_t)sz + 1);
        if (!data) { fclose(f); snprintf(out, outsz, "[edit_file] out of memory"); return; }
        size_t rd = fread(data, 1, (size_t)sz, f);
        (void)rd;
        fclose(f);
        data[sz] = '\0';
        char *hit = strstr(data, pt->search);
        if (!hit) { free(data); snprintf(out, outsz, "[edit_file] pattern not found: %s", pt->search); return; }
        size_t hoff = (size_t)(hit - data);
        size_t slen = strlen(pt->search);
        size_t clen = strlen(pt->content);
        char *newd = malloc((size_t)sz - slen + clen + 1);
        if (!newd) { free(data); snprintf(out, outsz, "[edit_file] out of memory"); return; }
        memcpy(newd, data, hoff);
        memcpy(newd + hoff, pt->content, clen);
        memcpy(newd + hoff + clen, hit + slen, (size_t)sz - hoff - slen + 1);
        free(data);
        FILE *wf = fopen(resolved, "wb");
        if (!wf) { free(newd); snprintf(out, outsz, "[edit_file] cannot write: %s", resolved); return; }
        fwrite(newd, 1, strlen(newd), wf);
        fclose(wf);
        free(newd);
        snprintf(out, outsz, "[edit_file] replaced in %s", resolved);
    } else {
        if (!pt->content[0]) { snprintf(out, outsz, "[edit_file] nothing to append"); return; }
        FILE *f = fopen(resolved, "ab");
        if (!f) { snprintf(out, outsz, "[edit_file] cannot open: %s", resolved); return; }
        size_t n = fwrite(pt->content, 1, strlen(pt->content), f);
        fclose(f);
        snprintf(out, outsz, "[edit_file] appended %zu bytes to %s", n, resolved);
    }
}

static void escape_single_quote(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 4 < outsz; i++) {
        if (in[i] == '\'') { memcpy(out + o, "'\\''", 4); o += 4; }
        else out[o++] = in[i];
    }
    out[o] = '\0';
}

static void tool_search(const PendingTool *pt, char *out, size_t outsz) {
    char q[MSG_LEN * 2], ts[PATH_BUF * 2], cmd[PATH_BUF * 4];
    escape_single_quote(pt->arg, q, sizeof(q));
    if (pt->search[0]) {
        char rp[PATH_BUF];
        if (pt->search[0] == '/') snprintf(rp, sizeof(rp), "%s", pt->search);
        else snprintf(rp, sizeof(rp), "%s/%s", g_house_root, pt->search);
        escape_single_quote(rp, ts, sizeof(ts));
        snprintf(cmd, sizeof(cmd), "grep -rn -- '%s' '%s' 2>&1 | head -30", q, ts);
    } else {
        escape_single_quote(g_house_root, ts, sizeof(ts));
        snprintf(cmd, sizeof(cmd), "grep -rn -- '%s' '%s' 2>&1 | head -30", q, ts);
    }
    FILE *pipe = popen(cmd, "r");
    if (!pipe) { snprintf(out, outsz, "[search_in_files] failed to start grep"); return; }
    char buf[MSG_LEN / 2];
    size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
    buf[n] = '\0';
    pclose(pipe);
    if (!n) snprintf(out, outsz, "[search_in_files] no matches for: %s", pt->arg);
    else snprintf(out, outsz, "[search_in_files] %s:\n%s", pt->arg, buf);
}

static void tool_exec(const PendingTool *pt, char *out, size_t outsz) {
    FILE *pipe = popen(pt->arg, "r");
    if (!pipe) { snprintf(out, outsz, "[cmd_exec] failed to start"); return; }
    char buf[MSG_LEN / 2];
    size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
    buf[n] = '\0';
    pclose(pipe);
    if (!n) snprintf(out, outsz, "[cmd_exec] OK (no output)");
    else snprintf(out, outsz, "[cmd_exec] %s\n%s", pt->arg, buf);
}

static void execute_pending_tool_into(const PendingTool *pt, char *out, size_t outsz) {
    if (strcmp(pt->name, "list_dir") == 0) tool_list_dir(pt->arg, out, outsz);
    else if (strcmp(pt->name, "read_file") == 0) tool_read_file(pt->arg, out, outsz);
    else if (strcmp(pt->name, "write_file") == 0) tool_write_file(pt, out, outsz);
    else if (strcmp(pt->name, "edit_file") == 0) tool_edit_file(pt, out, outsz);
    else if (strcmp(pt->name, "search_in_files") == 0) tool_search(pt, out, outsz);
    else if (strcmp(pt->name, "cmd_exec") == 0) tool_exec(pt, out, outsz);
    else snprintf(out, outsz, "[tool] unknown tool: %s", pt->name);
}

/* Fires a tool in a detached child (same non-blocking shape as the Ollama
 * curl child) so the render loop + thinking animation keep running. The
 * child runs the tool function directly and writes its text to the outfile. */
static void start_tool_job(PendingTool *pt) {
    if (g_pending) return;
    g_pending_tool = *pt;
    snprintf(g_pending_outfile, sizeof(g_pending_outfile), "%s/toolout-%d.txt", g_audit_dir, (int)getpid());
    unlink(g_pending_outfile);
    pid_t pid = fork();
    if (pid == 0) {
        char result[MSG_LEN];
        result[0] = '\0';
        execute_pending_tool_into(&g_pending_tool, result, sizeof(result));
        int fd = open(g_pending_outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { ssize_t wr = write(fd, result, strlen(result)); (void)wr; close(fd); }
        _exit(0);
    } else if (pid > 0) {
        g_pending = 1;
        g_pending_pid = pid;
        g_pending_is_tool = 1;
    }
}

static void tool_request_banner(const PendingTool *pt, char *out, size_t outsz) {
    char preview[48] = "";
    if (pt->content[0]) {
        snprintf(preview, sizeof(preview), "%.40s", pt->content);
        for (size_t i = 0; preview[i]; i++) if (preview[i] == '\n') preview[i] = ' ';
    }
    snprintf(out, outsz, "[tool request] %s %s%s", pt->name, pt->arg,
             preview[0] ? "  (+content: " : "");
    if (preview[0]) { size_t l = strlen(out); snprintf(out + l, outsz - l, "%s)", preview); }
    strncat(out, " - approve/deny in the sidebar", outsz - strlen(out) - 1);
}

/* ---------- input state ---------- */
static int g_armed = 0;
static char g_input_buf[INPUT_BUF_LEN] = "";
static int g_input_len = 0;
static int g_composer_h = COMPOSER_H; /* grows with wrapped input, recomputed each redraw() - see COMPOSER_MAX_LINES */

/* ---------- dynamic nav array (db-hq/events-hq convention, adapted to
 * this file's flat non-tag-tree layout) - rebuilt fresh every redraw
 * since the sidebar's session list can grow/shrink. ---------- */
typedef enum { NAV_NEWCHAT, NAV_SESSION, NAV_SCROLL_UP, NAV_SCROLL_DOWN, NAV_COMPOSER, NAV_CLOSE, NAV_TOOL_APPROVE, NAV_TOOL_DENY, NAV_MODEL, NAV_STATS } NavKind;
typedef struct { NavKind kind; int session_idx; int x, y, w, h; } NavItem;
static NavItem g_nav[MAX_NAV];
static int g_n_nav = 0;
static int g_focus_nav = 1;

static int nav_add(NavKind kind, int session_idx) {
    if (g_n_nav >= MAX_NAV) return -1;
    g_nav[g_n_nav].kind = kind;
    g_nav[g_n_nav].session_idx = session_idx;
    g_nav[g_n_nav].x = g_nav[g_n_nav].y = g_nav[g_n_nav].w = g_nav[g_n_nav].h = 0;
    g_n_nav++;
    return g_n_nav; /* 1-based nav index of the item just added */
}

/* REAL FIX 2026-08-12, direct report ("hai doesn't have mouse working
 * yet unlike db-hq"): db-hq has real click-to-select-and-activate hit
 * testing (khtpm_hq_render.c's own hit_test()/handle_click()) that
 * this file never got - ButtonPress here only ever checked the chrome
 * bar for window-dragging, nothing else. Every nav item now records
 * its own clickable rect right after nav_add() (same 1-based index),
 * so a real ButtonPress can hit-test against g_nav[] directly - see
 * handle_click() near main()'s event loop. */
static void nav_set_rect(int nav_index, int x, int y, int w, int h) {
    if (nav_index < 1 || nav_index > g_n_nav) return;
    g_nav[nav_index - 1].x = x; g_nav[nav_index - 1].y = y;
    g_nav[nav_index - 1].w = w; g_nav[nav_index - 1].h = h;
}

/* ---------- transcript scroll (ported convention - see file header
 * comment for the real source: wraith-alpha's fs scroll_offset/
 * VISIBLE_ENTRIES shape, NOT tpmos's separate joystick/GL scrollbar).
 * Unit is FLAT WRAPPED LINES (not messages, since messages vary in
 * wrapped height) - flattened fresh each redraw into g_flat_lines[]. */
static int g_scroll_offset = 0; /* 0 = following the bottom/newest */

typedef enum { FSTYLE_NORMAL = 0, FSTYLE_BULLET, FSTYLE_SUBTEXT } FlatStyle;
typedef struct { char text[512]; int is_header; const char *role; int style; } FlatLine;
static FlatLine g_flat[MAX_FLAT_LINES];
static int g_n_flat = 0;

/* ---------- window / GC / fonts ---------- */
static Display *dpy;
static Window win;
static GC gc;
static Pixmap buf;
static XftDraw *xftdraw_buf;
static Atom wm_delete;
static int g_win_x = 200, g_win_y = 280;
static int g_win_w = WIN_W, g_win_h = WIN_H;
static int g_dragging = 0, g_drag_start_x, g_drag_start_y, g_drag_win_x0, g_drag_win_y0;
static int g_resizing = 0, g_resize_start_x, g_resize_start_y, g_resize_w0, g_resize_h0;
#define RESIZE_GRIP 20
#define MIN_WIN_W 620
#define MIN_WIN_H 420

static XftFont *font_ui = NULL, *font_small = NULL, *font_mono = NULL;
static XftFont *font_ui_bold = NULL, *font_ui_italic = NULL;

static XftColor col_text, col_muted, col_accent, col_danger, col_user, col_assistant;
static XftColor col_user_bright, col_assistant_bright, col_bullet, col_subtext;
static Colormap cmap;
static Visual *vis;

static void load_fonts(void) {
    font_ui = XftFontOpenName(dpy, DefaultScreen(dpy), "Sans-9");
    font_small = XftFontOpenName(dpy, DefaultScreen(dpy), "Sans-8");
    font_mono = XftFontOpenName(dpy, DefaultScreen(dpy), "Monospace-9");
    font_ui_bold = XftFontOpenName(dpy, DefaultScreen(dpy), "Sans:bold:size=9");
    font_ui_italic = XftFontOpenName(dpy, DefaultScreen(dpy), "Sans:oblique:size=9");
    if (!font_ui_bold) font_ui_bold = font_ui;
    if (!font_ui_italic) font_ui_italic = font_ui;
}

/* Classifies a wrapped transcript line for the hierarchy typography
 * (bold/underline on bullet points, italic+dim on their indented
 * subtext): a BULLET line starts (after any indent) with a list marker
 * (- * + bullet en/em-dash or a numbered "1."/"1)"), a SUBTEXT line is
 * one that is indented and not itself a bullet, everything else NORMAL. */
static FlatStyle classify_line(const char *s) {
    if (!s) return FSTYLE_NORMAL;
    const char *p = s;
    int indented = 0;
    while (*p == ' ' || *p == '\t') { p++; indented = 1; }
    if (!*p) return FSTYLE_NORMAL;
    const unsigned char *u = (const unsigned char *)p;
    int is_dash = (*p == '-' || *p == '*' || *p == '+');
    int is_utf8_marker = (u[0] == 0xE2 && u[1] == 0x80 &&
                          (u[2] == 0xA2 || u[2] == 0xA5 || u[2] == 0xA6 ||
                           u[2] == 0x93 || u[2] == 0x94 || u[2] == 0x99));
    if (is_dash || is_utf8_marker) {
        const char *after = is_utf8_marker ? p + 3 : p + 1;
        if (*after == ' ' || *after == '\t' || !*after) return FSTYLE_BULLET;
    }
    if (*p >= '0' && *p <= '9') {
        const char *q = p + 1;
        while (*q >= '0' && *q <= '9') q++;
        if ((*q == '.' || *q == ')' || *q == ']') && (q[1] == ' ' || q[1] == '\t' || !q[1]))
            return FSTYLE_BULLET;
    }
    return indented ? FSTYLE_SUBTEXT : FSTYLE_NORMAL;
}

static void xft_color(const char *hex, XftColor *out) {
    XRenderColor rc = {0,0,0,0xffff};
    unsigned int r=0,g=0,b=0;
    if (hex[0] == '#') sscanf(hex+1, "%02x%02x%02x", &r,&g,&b);
    rc.red = (unsigned short)(r*257); rc.green = (unsigned short)(g*257); rc.blue = (unsigned short)(b*257);
    XftColorAllocValue(dpy, vis, cmap, &rc, out);
}

/* ---------- REAL emoji rendering (ported from chtpm's own pipeline,
 * direct instruction: "chtpm uses a function to convert emoji to .csv
 * first use that"). The .csv is made once per emoji by chtpm's
 * emoji_gen_atlas (FreeType-rasterizes one emoji -> 64x64 PNG) followed
 * by emoji_xtract (-> 16x16 r,g,b,a voxel CSV), dropped into
 * <house>/&.widgits/ai-cell/pieces/registry/emoji_assets/<hex-cp>/
 * voxels_16.csv - pre-generated, not per-frame, exactly like
 * chtpm_rgb_render.c's own load_emoji_voxels()/blit_emoji_tile(). At
 * runtime we load those CSVs once and blit the tiles inline while
 * Xft draws the plain-text runs around them. ---------- */
#define EMOJI_TILE 16
#define EMOJI_ADV 18 /* advance per emoji, keeps wrap math in text_width() honest */
typedef struct {
    unsigned int cp;                          /* base unicode codepoint (hex dir name) */
    unsigned char px[EMOJI_TILE * EMOJI_TILE * 4]; /* r,g,b,a per pixel */
} EmojiTile;
static EmojiTile g_emoji_tiles[512];
static int g_emoji_n = 0;
static int g_px_rshift, g_px_gshift, g_px_bshift; /* TrueColor packing (vis masks) */

static int utf8_decode(const unsigned char *s, unsigned int *cp) {
    if (s[0] < 0x80) { *cp = s[0]; return 1; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) { *cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); return 2; }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) { *cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); return 3; }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) { *cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); return 4; }
    *cp = 0xFFFD; return 1;
}

static int mask_shift(unsigned long m) {
    int s = 0;
    while (m && !(m & 1UL)) { m >>= 1; s++; }
    return s;
}

static int load_emoji_tiles(void) {
    DIR *d = opendir(g_emoji_dir);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) && g_emoji_n < 512) {
        if (e->d_name[0] == '.') continue;
        char csv[PATH_BUF];
        snprintf(csv, sizeof(csv), "%s/%s/voxels_16.csv", g_emoji_dir, e->d_name);
        FILE *f = fopen(csv, "r");
        if (!f) continue;
        unsigned int cp = (unsigned int)strtoul(e->d_name, NULL, 16);
        EmojiTile *t = &g_emoji_tiles[g_emoji_n];
        memset(t, 0, sizeof(*t));
        t->cp = cp;
        char line[64];
        int npix = 0;
        while (fgets(line, sizeof(line), f) && npix < EMOJI_TILE * EMOJI_TILE) {
            if (line[0] == '#' || line[0] == '\n') continue;
            if (line[0] == 'r' && line[1] == ',') continue; /* "r,g,b,a" header */
            int r, g, b, a;
            if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
                size_t o = (size_t)npix * 4;
                t->px[o] = (unsigned char)r; t->px[o + 1] = (unsigned char)g;
                t->px[o + 2] = (unsigned char)b; t->px[o + 3] = (unsigned char)a;
                npix++;
            }
        }
        fclose(f);
        if (npix == EMOJI_TILE * EMOJI_TILE) g_emoji_n++;
    }
    closedir(d);
    return g_emoji_n;
}

static const EmojiTile *emoji_for_cp(unsigned int cp) {
    for (int i = 0; i < g_emoji_n; i++) if (g_emoji_tiles[i].cp == cp) return &g_emoji_tiles[i];
    return NULL;
}

static void blit_emoji_tile(const EmojiTile *t, int x, int ytop) {
    for (int yy = 0; yy < EMOJI_TILE; yy++) {
        for (int xx = 0; xx < EMOJI_TILE; xx++) {
            size_t o = ((size_t)yy * EMOJI_TILE + xx) * 4;
            if (t->px[o + 3] < 128) continue; /* skip transparent pixels */
            unsigned long px = ((((unsigned long)t->px[o]) << g_px_rshift) & vis->red_mask) |
                               ((((unsigned long)t->px[o + 1]) << g_px_gshift) & vis->green_mask) |
                               ((((unsigned long)t->px[o + 2]) << g_px_bshift) & vis->blue_mask);
            XSetForeground(dpy, gc, px);
            XDrawPoint(dpy, buf, gc, x + xx, ytop + yy);
        }
    }
}

/* text -> drawable segments (text runs vs emoji tiles); zero-width
 * joiners / variation selectors / ZWSP are consumed silently so no
 * tofu box ever renders for them. */
typedef struct { const char *s; int len; int is_emoji; const EmojiTile *tile; } DrawSeg;
static int build_segs(const char *text, DrawSeg *segs, int maxsegs) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)text;
    const unsigned char *run = p;
    while (*p) {
        unsigned int cp; int clen = utf8_decode(p, &cp);
        int zero_w = (cp == 0xFE0F || cp == 0x200D || cp == 0x200C || cp == 0x200B);
        const EmojiTile *t = zero_w ? NULL : emoji_for_cp(cp);
        if (t || zero_w) {
            if (p > run && n < maxsegs) { segs[n].s = (const char *)run; segs[n].len = (int)(p - run); segs[n].is_emoji = 0; segs[n].tile = NULL; n++; }
            if (t && n < maxsegs) { segs[n].s = (const char *)p; segs[n].len = clen; segs[n].is_emoji = 1; segs[n].tile = t; n++; }
            p += clen; run = p;
        } else {
            p += clen;
        }
    }
    if (p > run && n < maxsegs) { segs[n].s = (const char *)run; segs[n].len = (int)(p - run); segs[n].is_emoji = 0; segs[n].tile = NULL; n++; }
    return n;
}

static int text_run_advance(XftFont *f, const char *s, int len) {
    XGlyphInfo gi;
    XftTextExtentsUtf8(dpy, f, (const FcChar8 *)s, len, &gi);
    return gi.xOff;
}

static void draw_text(XftFont *f, XftColor *c, int x, int y, const char *s) {
    if (!s || !*s) return;
    DrawSeg segs[512];
    int n = build_segs(s, segs, 512);
    int sx = x;
    int tile_top = y - 13; /* baseline y; tile sits up in the line box */
    for (int i = 0; i < n; i++) {
        if (segs[i].is_emoji) {
            blit_emoji_tile(segs[i].tile, sx, tile_top);
            sx += EMOJI_ADV;
        } else {
            XftDrawStringUtf8(xftdraw_buf, c, f, sx, y, (const FcChar8 *)segs[i].s, segs[i].len);
            sx += (int)text_run_advance(f, segs[i].s, segs[i].len);
        }
    }
}

static int text_width(XftFont *f, const char *s) {
    DrawSeg segs[512];
    int n = build_segs(s, segs, 512);
    int w = 0;
    for (int i = 0; i < n; i++) {
        if (segs[i].is_emoji) w += EMOJI_ADV;
        else w += text_run_advance(f, segs[i].s, segs[i].len);
    }
    return w;
}

/* word-wraps a message body into `out[]` lines of at most max_px width,
 * returns line count. Simple greedy wrap, good enough for chat text. */
static int wrap_text(XftFont *f, const char *text, int max_px, char out[][512], int max_lines) {
    int n = 0;
    char word[256];
    char line[512] = "";
    const char *p = text;
    while (*p && n < max_lines) {
        int wi = 0;
        while (*p && *p != ' ' && *p != '\n' && wi < 255) word[wi++] = *p++;
        word[wi] = '\0';
        int hard_break = (*p == '\n');
        char trial[600];
        if (line[0]) snprintf(trial, sizeof(trial), "%s %s", line, word);
        else snprintf(trial, sizeof(trial), "%s", word);
        if (text_width(f, trial) > max_px && line[0]) {
            snprintf(out[n++], 512, "%s", line);
            snprintf(line, sizeof(line), "%s", word);
        } else {
            snprintf(line, sizeof(line), "%s", trial);
        }
        if (*p == ' ') p++;
        if (hard_break) {
            if (n < max_lines) snprintf(out[n++], 512, "%s", line);
            line[0] = '\0';
            p++;
        }
    }
    if (line[0] && n < max_lines) snprintf(out[n++], 512, "%s", line);
    return n;
}

/* Flattens g_msgs into g_flat[] (one entry per wrapped visual line,
 * plus a role-header entry before each message) - the scroll window
 * indexes into THIS array, so scroll units are stable regardless of
 * variable message heights. Must be called before either drawing the
 * transcript or computing scroll clamps. */
static void flatten_transcript(int max_px) {
    g_n_flat = 0;
    for (int i = 0; i < g_n_msgs && g_n_flat < MAX_FLAT_LINES - 40; i++) {
        if (g_n_flat < MAX_FLAT_LINES) {
            g_flat[g_n_flat].is_header = 1;
            g_flat[g_n_flat].role = g_msgs[i].is_user ? "You" : "ai-cell";
            g_flat[g_n_flat].text[0] = '\0';
            g_flat[g_n_flat].style = FSTYLE_NORMAL;
            g_n_flat++;
        }
        char lines[32][512];
        int nlines = wrap_text(font_ui, g_msgs[i].text, max_px, lines, 32);
        for (int l = 0; l < nlines && g_n_flat < MAX_FLAT_LINES; l++) {
            g_flat[g_n_flat].is_header = 0;
            g_flat[g_n_flat].role = NULL;
            g_flat[g_n_flat].style = classify_line(lines[l]);
            snprintf(g_flat[g_n_flat].text, sizeof(g_flat[g_n_flat].text), "%s", lines[l]);
            g_n_flat++;
        }
        if (g_n_flat < MAX_FLAT_LINES) { g_flat[g_n_flat].is_header = 0; g_flat[g_n_flat].role = NULL; g_flat[g_n_flat].text[0] = '\0'; g_flat[g_n_flat].style = FSTYLE_NORMAL; g_n_flat++; } /* blank spacer line between messages */
    }
}

static void submit_composer(void) {
    if (!g_input_len) return;
    if (g_pending) {
        add_and_persist(0, "[h-ai busy - previous request still running, send again when it finishes]");
        g_input_buf[0] = '\0';
        g_input_len = 0;
        g_armed = 0;
        return;
    }
    if (g_tool_pending) { /* resolve the pending approve/deny first */
        add_and_persist(0, "[a tool request is awaiting approve/deny in the sidebar]");
        g_input_buf[0] = '\0';
        g_input_len = 0;
        g_armed = 0;
        return;
    }

    char prompt[INPUT_BUF_LEN];
    snprintf(prompt, sizeof(prompt), "%s", g_input_buf);
    add_and_persist(1, prompt);

    PendingTool pt;
    if (detect_tool(prompt, &pt)) {
        if (tool_requires_approval(pt.name)) {
            g_pending_tool = pt;
            g_tool_pending = 1;
            char banner[MSG_LEN];
            tool_request_banner(&pt, banner, sizeof(banner));
            add_and_persist(0, banner);
        } else {
            start_tool_job(&pt); /* read-only: pre-execute (agent-45 Strategy A) */
        }
    } else {
        send_to_ollama(prompt);
    }

    g_input_buf[0] = '\0';
    g_input_len = 0;
    g_armed = 0;
    g_scroll_follow_bottom = 1;
}

static void new_chat(void) {
    start_new_session();
    g_input_buf[0] = '\0';
    g_input_len = 0;
    g_scroll_follow_bottom = 1;
    refresh_sessions();
}

/* ---------- draw ---------- */
static void draw_badge(int nav_index, int is_scroll_arrow, int x, int y);

/* REAL FIX 2026-08-12, direct instruction ("don't use esc to close.
 * use a real x nav as is standard"): closing used to be Escape-only,
 * not the real nav-indexed close convention every other khtpm window
 * (db-hq, events-hq) already uses - a real, numbered, clickable/
 * digit-jumpable close button in the chrome bar, same as "close button
 * gets the LAST nav index" fixed for db-hq earlier this same session
 * (see !.HOUSE_STDS.md - "everything gets a number", including close).
 * Escape now only disarms the composer (same as before), it does NOT
 * close the window anymore - use the real close button/nav item like
 * every other window here. */
static void draw_chrome_bar(void) {
    XSetForeground(dpy, gc, 0x1a1a1a);
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, CHROME_H);
    draw_text(font_small, &col_muted, 10, CHROME_H - 9, "ai-cell — [Backspace] on a chat row deletes it");
}

/* Close button gets the LAST nav index, drawn far right of the chrome
 * bar - called at the very end of redraw()'s nav-building pass, after
 * every other real nav item already has its index, matching db-hq's
 * own established close-button convention exactly. */
/* REAL FIX 2026-08-12, direct correction ("db-hq and hai are using
 * nav index in not quite the std the std is [].<#> not [<#>]"):
 * verified against the actual real reference
 * (1.TPMOS_c_+rmmp.0103.0001/projects/wraith-alpha/ops/
 * wraith_parser_alpha.c ~line 2221-2224/2283) - the bracket holds
 * ONLY the state glyph (`[^]`/`[>]`/`[]`/`[ ]`), the number is a
 * SEPARATE suffix drawn after the closing bracket with a trailing
 * period (e.g. `[>]1.`), NOT embedded inside the brackets as `[>1]`.
 * Same fix applied to khtpm_hq_render.c (db-hq) and
 * khtpm_events_hq_render.c (events-hq) - all three now match the real
 * standard, see !.HOUSE_STDS.md #22's own correction. */
static void draw_close_button(void) {
    int nav_close = nav_add(NAV_CLOSE, -1);
    nav_set_rect(nav_close, g_win_w - 60, 0, 60, CHROME_H);
    char badge[24];
    int on = (nav_close == g_focus_nav);
    snprintf(badge, sizeof(badge), "[%s]%d. x", on ? ">" : " ", nav_close);
    draw_text(font_small, on ? &col_danger : &col_muted, g_win_w - 56, CHROME_H - 9, badge);
}

static void draw_badge(int nav_index, int is_scroll_arrow, int x, int y) {
    char badge[16];
    int on = (nav_index == g_focus_nav);
    snprintf(badge, sizeof(badge), "[%s]%d.", on ? (g_armed && !is_scroll_arrow ? "^" : ">") : " ", nav_index);
    XftColor *c = on ? &col_accent : &col_text;
    draw_text(font_small, c, x, y, badge);
}


/* Model persistence and cycling */
static void load_model_choice(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/model.txt", g_sessions_root);
    FILE *f = fopen(path, "r");
    if (!f) {
        strncpy(g_model_name, g_models[0].name, sizeof(g_model_name) - 1);
        g_model_idx = 0;
        g_backend_mode = g_models[0].mode;
        return;
    }
    char buf[128];
    if (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
        for (int i = 0; i < g_n_models; i++) {
            if (strcmp(buf, g_models[i].name) == 0) {
                g_model_idx = i;
                strncpy(g_model_name, g_models[i].name, sizeof(g_model_name) - 1);
                g_backend_mode = g_models[i].mode;
                fclose(f);
                return;
            }
        }
    }
    fclose(f);
    strncpy(g_model_name, g_models[0].name, sizeof(g_model_name) - 1);
    g_model_idx = 0;
    g_backend_mode = g_models[0].mode;
}

static void save_model_choice(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/model.txt", g_sessions_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", g_model_name);
        fclose(f);
    }
}

static void cycle_model(void) {
    g_model_idx = (g_model_idx + 1) % g_n_models;
    strncpy(g_model_name, g_models[g_model_idx].name, sizeof(g_model_name) - 1);
    g_backend_mode = g_models[g_model_idx].mode;
    save_model_choice();
}

static void draw_sidebar(void) {
    XSetForeground(dpy, gc, 0x141414);
    XFillRectangle(dpy, buf, gc, 0, CHROME_H, SIDEBAR_W, (unsigned)(g_win_h - CHROME_H));

    int y = CHROME_H + 18;
    int nav_newchat = nav_add(NAV_NEWCHAT, -1);
    nav_set_rect(nav_newchat, 0, y - 14, SIDEBAR_W, 24);
    draw_badge(nav_newchat, 0, 14, y);
    draw_text(font_ui, nav_newchat == g_focus_nav ? &col_accent : &col_text, 62, y, "+ New chat");
    y += 30;

    if (g_tool_pending) {
        int nav_app = nav_add(NAV_TOOL_APPROVE, -1);
        nav_set_rect(nav_app, 0, y - 14, SIDEBAR_W, 24);
        draw_badge(nav_app, 0, 14, y);
        char lbl[120];
        snprintf(lbl, sizeof(lbl), "Approve: %s", g_pending_tool.name);
        draw_text(font_ui, nav_app == g_focus_nav ? &col_accent : &col_text, 62, y, lbl);
        y += 24;
        int nav_deny = nav_add(NAV_TOOL_DENY, -1);
        nav_set_rect(nav_deny, 0, y - 14, SIDEBAR_W, 24);
        draw_badge(nav_deny, 0, 14, y);
        draw_text(font_ui, nav_deny == g_focus_nav ? &col_danger : &col_text, 62, y, "Deny");
        y += 26;
    }

    XSetForeground(dpy, gc, 0x2a2a2a);
    XDrawLine(dpy, buf, gc, 0, y, SIDEBAR_W, y);
    y += 18;

    draw_text(font_small, &col_muted, 14, y, "HISTORY (audit — Backspace deletes)");
    y += 20;

    for (int i = 0; i < g_n_sessions && y < g_win_h - 90; i++) {
        int nav_i = nav_add(NAV_SESSION, i);
        nav_set_rect(nav_i, 0, y - 14, SIDEBAR_W, 20);
        int is_current = (g_session_dir[0] && strcmp(g_sessions[i].dir, g_session_dir) == 0);
        draw_badge(nav_i, 0, 14, y);
        XftColor *rc = (nav_i == g_focus_nav) ? &col_accent : (is_current ? &col_text : &col_muted);
        char lbl[100];
        snprintf(lbl, sizeof(lbl), "%s%s", is_current ? "* " : "", g_sessions[i].label);
        draw_text(font_small, rc, 62, y, lbl);
        y += 20;
    }
    if (g_n_sessions == 0) {
        draw_text(font_small, &col_muted, 14, y, "(no saved chats yet)");
        y += 20;
    }

    y = g_win_h - 70;
    XSetForeground(dpy, gc, 0x2a2a2a);
    XDrawLine(dpy, buf, gc, 0, y, SIDEBAR_W, y);
    y += 16;
    draw_text(font_small, &col_muted, 14, y, "MODEL");
    y += 18;
    int nav_model = nav_add(NAV_MODEL, -1);
    nav_set_rect(nav_model, 0, y - 14, SIDEBAR_W, 24);
    draw_badge(nav_model, 0, 14, y);
    draw_text(font_small, nav_model == g_focus_nav ? &col_accent : &col_text, 62, y, g_model_name);
    y -= 28;
    int nav_stats = nav_add(NAV_STATS, -1);
    nav_set_rect(nav_stats, 0, y - 14, SIDEBAR_W, 24);
    draw_badge(nav_stats, 0, 14, y);
    draw_text(font_small, nav_stats == g_focus_nav ? &col_accent : &col_text, 62, y, "Stats");
}

static void draw_topbar(void) {
    XSetForeground(dpy, gc, 0x0d0d0d); /* darker than the transcript/sidebar below it (user: "toolbar at top should be darker") */
    XFillRectangle(dpy, buf, gc, SIDEBAR_W, CHROME_H, (unsigned)(g_win_w - SIDEBAR_W), TOPBAR_H);
    XSetForeground(dpy, gc, 0x2a2a2a);
    XDrawLine(dpy, buf, gc, SIDEBAR_W, CHROME_H + TOPBAR_H, g_win_w, CHROME_H + TOPBAR_H);
    draw_text(font_ui, &col_text, SIDEBAR_W + 16, CHROME_H + 27, "h-ai");
    /* thinking animation: cycling dots while a request or tool is in
     * flight (g_frame is bumped every redraw, so this visibly animates) */
    if (g_pending) {
        static const char *dots[] = {".", "..", "...", ""};
        char anim[32];
        snprintf(anim, sizeof(anim), "thinking%s", dots[(g_frame / 6) % 4]);
        draw_text(font_small, g_pending_is_tool ? &col_assistant : &col_muted, g_win_w - 130, CHROME_H + 27, anim);
    }
}

static int transcript_geom(int *x0, int *y0, int *w, int *h) {
    *x0 = SIDEBAR_W; *y0 = CHROME_H + TOPBAR_H;
    *w = g_win_w - SIDEBAR_W; *h = g_win_h - CHROME_H - TOPBAR_H - g_composer_h;
    return (*h - 8) / LINE_H; /* visible line count, leaving room for the scroll-arrow row */
}

static void draw_transcript(void) {
    int x0, y0, w, h;
    int visible = transcript_geom(&x0, &y0, &w, &h) - 1; /* -1 for the arrow row */
    XSetForeground(dpy, gc, 0x1a1a1a);
    XFillRectangle(dpy, buf, gc, x0, y0, (unsigned)w, (unsigned)h);

    flatten_transcript(w - 32);
    int max_scroll = g_n_flat > visible ? g_n_flat - visible : 0;
    if (g_scroll_follow_bottom) g_scroll_offset = max_scroll;
    if (g_scroll_offset > max_scroll) g_scroll_offset = max_scroll;
    if (g_scroll_offset < 0) g_scroll_offset = 0;

    /* scroll-arrow row (ported convention - bare [▲]/[▼] nav buttons,
     * see file header comment) */
    int nav_up = nav_add(NAV_SCROLL_UP, -1);
    int nav_down = nav_add(NAV_SCROLL_DOWN, -1);
    int ay = y0 + 14;
    nav_set_rect(nav_up, x0 + 8, ay - 12, 90, 20);
    nav_set_rect(nav_down, x0 + 106, ay - 12, 90, 20);
    draw_badge(nav_up, 1, x0 + 12, ay);
    draw_text(font_small, nav_up == g_focus_nav ? &col_accent : &col_muted, x0 + 56, ay, "up");
    draw_badge(nav_down, 1, x0 + 110, ay);
    draw_text(font_small, nav_down == g_focus_nav ? &col_accent : &col_muted, x0 + 148, ay, "down");
    char pos[64];
    snprintf(pos, sizeof(pos), "line %d-%d / %d", g_scroll_offset + 1,
             g_scroll_offset + (visible < g_n_flat ? visible : g_n_flat), g_n_flat);
    draw_text(font_small, &col_muted, x0 + w - 140, ay, pos);

    int y = y0 + 34;
    const char *cur_role = NULL;
    for (int i = g_scroll_offset; i < g_n_flat && i < g_scroll_offset + visible; i++) {
        if (g_flat[i].is_header) {
            cur_role = g_flat[i].role;
            draw_text(font_small, &col_muted, x0 + 16, y, g_flat[i].role);
        } else if (g_flat[i].text[0]) {
            XftColor *mc = (cur_role && strcmp(cur_role, "You") == 0)
                               ? &col_user
                               : (cur_role && strcmp(cur_role, "ai-cell") == 0)
                                     ? &col_assistant
                                     : &col_text;
            XftFont *mf = font_ui;
            if (g_flat[i].style == FSTYLE_BULLET) {
                mf = font_ui_bold;
                mc = (cur_role && strcmp(cur_role, "You") == 0) ? &col_user_bright
                   : (cur_role && strcmp(cur_role, "ai-cell") == 0) ? &col_assistant_bright
                                                                   : &col_bullet;
            } else if (g_flat[i].style == FSTYLE_SUBTEXT) {
                mf = font_ui_italic;
                mc = &col_subtext;
            }
            draw_text(mf, mc, x0 + 16, y, g_flat[i].text);
            if (g_flat[i].style == FSTYLE_BULLET) {
                XSetForeground(dpy, gc, mc->pixel);
                XFillRectangle(dpy, buf, gc, x0 + 16, y + 4,
                               (unsigned)text_width(mf, g_flat[i].text), 1);
            }
        }
        y += LINE_H;
    }
}

/* Available pixel width for wrapped composer text - same margins
 * draw_composer() itself uses (70px left for the nav badge, 20px
 * right breathing room), computed once so update_composer_height()
 * (called BEFORE draw_composer() in redraw(), to size the box that
 * frame) and draw_composer() itself always wrap identically. */
static int composer_wrap_px(void) {
    return (g_win_w - SIDEBAR_W) - 70 - 20;
}

/* Recomputes g_composer_h from the CURRENT input text, called at the
 * top of redraw() before transcript_geom()/draw_transcript() so the
 * transcript's own visible-area math accounts for the composer's real
 * height THIS frame, not last frame's. Grows the box up to
 * COMPOSER_MAX_LINES tall as the input wraps to more lines; beyond
 * that the box stays capped and draw_composer() auto-scrolls to keep
 * the cursor's line visible instead of the input running off the
 * edge - real fix for the direct report "text input i want it to wrap
 * new line and user input can scroll up instead of dissapearing off
 * the side of the screen" (2026-08-13). */
static void update_composer_height(void) {
    if (!g_input_len) { g_composer_h = COMPOSER_H; return; }
    char lines[64][512];
    int nlines = wrap_text(font_ui, g_input_buf, composer_wrap_px(), lines, 64);
    if (nlines < 1) nlines = 1;
    int visible = nlines < COMPOSER_MAX_LINES ? nlines : COMPOSER_MAX_LINES;
    g_composer_h = COMPOSER_H + (visible - 1) * LINE_H;
}

static void draw_composer(void) {
    int x0 = SIDEBAR_W, y0 = g_win_h - g_composer_h;
    int w = g_win_w - SIDEBAR_W;
    XSetForeground(dpy, gc, 0x242424);
    XFillRectangle(dpy, buf, gc, x0 + 12, y0 + 10, (unsigned)(w - 24), (unsigned)(g_composer_h - 20));

    int nav_composer = nav_add(NAV_COMPOSER, -1);
    nav_set_rect(nav_composer, x0 + 12, y0 + 10, w - 24, g_composer_h - 20);
    draw_badge(nav_composer, 0, x0 + 20, y0 + 26);

    if (!g_input_len) {
        const char *placeholder = g_armed ? "" : "Enter to type a message...";
        draw_text(font_ui, &col_muted, x0 + 70, y0 + 26, placeholder);
        if (g_armed) {
            XSetForeground(dpy, gc, 0x22c55e);
            XFillRectangle(dpy, buf, gc, x0 + 70 + 2, y0 + 14, 2, 16);
        }
        return;
    }

    char lines[64][512];
    int nlines = wrap_text(font_ui, g_input_buf, composer_wrap_px(), lines, 64);
    if (nlines < 1) nlines = 1;
    int visible = nlines < COMPOSER_MAX_LINES ? nlines : COMPOSER_MAX_LINES;
    int first = nlines - visible; /* auto-follow-bottom: always show the tail, cursor stays visible */

    int ly = y0 + 26;
    for (int i = first; i < nlines; i++) {
        draw_text(font_ui, &col_text, x0 + 70, ly, lines[i]);
        ly += LINE_H;
    }

    if (g_armed) {
        int cx = x0 + 70 + text_width(font_ui, lines[nlines - 1]);
        int cy = y0 + 26 + (visible - 1) * LINE_H;
        XSetForeground(dpy, gc, 0x22c55e);
        XFillRectangle(dpy, buf, gc, cx + 2, cy - 12, 2, 16);
    }
}

/* REAL FIX 2026-08-12, direct instruction ("u should use png dump not
 * pil capture. from now on (or receipt) learn 2 rely on receipts"):
 * external xwd/PIL screen capture is unreliable once the real user is
 * actively using their own desktop - a window can be dragged, occluded,
 * or mid-composite exactly when a capture fires, producing bleed-
 * through from whatever's on top (confirmed live this session: an
 * xwd capture returned another real window's content once ai-cell got
 * covered). This app's OWN offscreen `buf` pixmap is the actual source
 * of truth - dumping FROM there (same technique db-hq's own
 * dump_frame_png() already proved: standard 0xRRGGBB byte layout, NOT
 * the zeroed mask fields XGetImage returns on a bare Pixmap) can never
 * race with window stacking/occlusion, because it reads what THIS
 * PROCESS drew, not what's visually on screen. A receipt file
 * (matching the `*.receipt.txt` convention already seen elsewhere in
 * this house, e.g. agent-45's own `rgb_frame.receipt.txt`) is written
 * right after, so a caller can poll for ITS existence/mtime instead of
 * guessing a sleep duration or trusting the PNG file's own write to be
 * atomic-enough to read mid-write. */
/* Real, human-readable label for g_nav[idx] (0-based), matching
 * EXACTLY what draw_sidebar()/draw_close_button()/draw_composer()
 * already draw on screen for that item - not a separate guess at what
 * the label "should" be. This is the missing piece flagged in
 * au11-hq/HARNESS-DELEGATION-PIPELINE.md §6 (nav_intent_to_index.sh
 * has no live label source yet) - added 2026-08-13 so a delegated
 * navigation decision (model names an item in plain text) can be
 * resolved against the REAL current labels instead of a hardcoded
 * guess, same "labels/order both drift, always read live" discipline
 * as everything else in this house's nav handling. */
static void nav_label(int idx, char *out, size_t outsz) {
    if (idx < 0 || idx >= g_n_nav) { out[0] = '\0'; return; }
    NavItem *it = &g_nav[idx];
    switch (it->kind) {
        case NAV_NEWCHAT: snprintf(out, outsz, "New Chat"); break;
        case NAV_TOOL_APPROVE: snprintf(out, outsz, "Approve: %s", g_pending_tool.name); break;
        case NAV_TOOL_DENY: snprintf(out, outsz, "Deny"); break;
        case NAV_SESSION:
            if (it->session_idx >= 0 && it->session_idx < g_n_sessions) {
                int is_current = (g_session_dir[0] && strcmp(g_sessions[it->session_idx].dir, g_session_dir) == 0);
                snprintf(out, outsz, "%s%s", is_current ? "* " : "", g_sessions[it->session_idx].label);
            } else snprintf(out, outsz, "Session");
            break;
        case NAV_MODEL: snprintf(out, outsz, "Model: %s", g_model_name); break;
        case NAV_SCROLL_UP: snprintf(out, outsz, "Scroll Up"); break;
        case NAV_SCROLL_DOWN: snprintf(out, outsz, "Scroll Down"); break;
        case NAV_COMPOSER: snprintf(out, outsz, "Composer"); break;
        case NAV_STATS: snprintf(out, outsz, "Stats"); break;
        case NAV_CLOSE: snprintf(out, outsz, "Close"); break;
        default: snprintf(out, outsz, "?"); break;
    }
}

/* Delegation-safe variant of nav_label(): same real label for every
 * kind EXCEPT NAV_SESSION, where the human-facing label embeds
 * arbitrary chat snippet text (e.g. "08-13 03:00 howdy , how are u?")
 * that can and did confuse a small model into echoing noise back
 * instead of naming a real item (HARNESS-DELEGATION-PIPELINE.md §6,
 * live finding 2026-08-13: gemma3:1b replied "Howdy" - resolver failed
 * closed correctly, but the label itself was the real problem). Fixed
 * by giving sessions a clean, content-free ordinal name for the
 * delegation-facing label ONLY - the human-facing nav_label() /
 * on-screen sidebar text is UNCHANGED, still shows the real snippet,
 * since a human benefits from that context and isn't confused by it
 * the way a small model is. */
static void nav_label_delegate_safe(int idx, char *out, size_t outsz) {
    if (idx < 0 || idx >= g_n_nav) { out[0] = '\0'; return; }
    NavItem *it = &g_nav[idx];
    if (it->kind == NAV_SESSION) {
        snprintf(out, outsz, "Session %d", it->session_idx + 1);
        return;
    }
    nav_label(idx, out, outsz);
}

/* Companion file to the PNG receipt: one REAL current nav label per
 * line, 1-based index implied by line number - the live source
 * nav_intent_to_index.sh needs (see that script's own header). Written
 * alongside the receipt on every dump (same 'p' relay trigger), not a
 * separate dump mode - one receipt read gets both counts and labels.
 * Two columns after the index: the human-facing display label (real
 * on-screen text, may contain snippet content), then the delegation-
 * safe label (content-free for noisy kinds like sessions) - a
 * delegation harness should resolve against the THIRD field, a human
 * reading this file for debugging wants the second. */
static void dump_nav_labels(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/ai-cell-frame.png.nav-labels.txt", g_audit_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    char lbl[128], safe_lbl[128];
    for (int i = 0; i < g_n_nav; i++) {
        nav_label(i, lbl, sizeof(lbl));
        nav_label_delegate_safe(i, safe_lbl, sizeof(safe_lbl));
        fprintf(f, "%d|%s|%s\n", i + 1, lbl, safe_lbl);
    }
    fclose(f);
}

static void dump_frame_png(void) {
    char frame_path_png[PATH_BUF];
    char frame_path_receipt[PATH_BUF];
    snprintf(frame_path_png, sizeof(frame_path_png), "%s/ai-cell-frame.png", g_audit_dir);
    snprintf(frame_path_receipt, sizeof(frame_path_receipt), "%s/ai-cell-frame.png.receipt.txt", g_audit_dir);
    dump_nav_labels();
    XSync(dpy, False);
    XImage *img = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
    if (!img) { fprintf(stderr, "ai-cell: dump_frame_png: XGetImage failed\n"); return; }
    int w = g_win_w, h = g_win_h;
    unsigned char *rgb = malloc((size_t)w * h * 3);
    if (!rgb) { XDestroyImage(img); return; }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned long px = XGetPixel(img, x, y);
            size_t o = ((size_t)y * w + x) * 3;
            rgb[o] = (unsigned char)((px >> 16) & 0xff);
            rgb[o + 1] = (unsigned char)((px >> 8) & 0xff);
            rgb[o + 2] = (unsigned char)(px & 0xff);
        }
    }
    XDestroyImage(img);
    int ok = stbi_write_png(frame_path_png, w, h, 3, rgb, w * 3);
    free(rgb);
    FILE *rf = fopen(frame_path_receipt, "w");
    if (rf) {
        fprintf(rf, "ok=%d w=%d h=%d t=%ld nav=%d n_nav=%d n_sessions=%d n_msgs=%d tool_pending=%d tool=%s\n",
                ok, w, h, (long)time(NULL), g_focus_nav, g_n_nav, g_n_sessions, g_n_msgs,
                g_tool_pending, g_tool_pending ? g_pending_tool.name : "none");
        fclose(rf);
    }
    fprintf(stderr, ok ? "ai-cell: wrote %s (%dx%d)\n" : "ai-cell: dump_frame_png: write failed\n", frame_path_png, w, h);
}

static void draw_resize_grip(void) {
    int x0 = g_win_w - RESIZE_GRIP - 6, y0 = g_win_h - RESIZE_GRIP - 6;
    XSetForeground(dpy, gc, 0x3a3a3a);
    XDrawLine(dpy, buf, gc, x0, y0 + 14, x0 + 14, y0);
    XDrawLine(dpy, buf, gc, x0, y0 + 8, x0 + 8, y0);
    XDrawLine(dpy, buf, gc, x0 + 6, y0 + 14, x0 + 14, y0 + 6);
}

static void redraw(void) {
    check_pending();
    g_n_nav = 0; /* rebuilt fresh below - db-hq/events-hq convention */
    g_frame++;

    update_composer_height(); /* before draw_transcript() - its own visible-area math (transcript_geom()) needs g_composer_h for THIS frame, not last frame's */

    XSetForeground(dpy, gc, 0x141414);
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
    draw_chrome_bar();
    draw_sidebar();
    draw_topbar();
    draw_transcript(); /* also (re)builds the flat-line cache used for scroll math */
    draw_composer();
    draw_close_button(); /* LAST nav index, drawn after every other real nav item exists */
    draw_resize_grip();

    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
    if (g_focus_nav < 1) g_focus_nav = 1;

    XSync(dpy, False);
    XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
    if (frame) {
        XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
        XDestroyImage(frame);
    } else {
        XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, 0, 0);
    }
    XFlush(dpy);
}

/* ---------- input ---------- */
static void activate_focused(void) {
    if (g_focus_nav < 1 || g_focus_nav > g_n_nav) return;
    NavItem *it = &g_nav[g_focus_nav - 1];
    switch (it->kind) {
        case NAV_NEWCHAT:
            new_chat();
            break;
        case NAV_SESSION:
            if (it->session_idx >= 0 && it->session_idx < g_n_sessions) {
                load_session(g_sessions[it->session_idx].dir);
                g_scroll_follow_bottom = 1;
            }
            break;
        case NAV_SCROLL_UP:
            g_scroll_follow_bottom = 0;
            g_scroll_offset -= 3;
            if (g_scroll_offset < 0) g_scroll_offset = 0;
            break;
        case NAV_SCROLL_DOWN:
            g_scroll_follow_bottom = 0;
            g_scroll_offset += 3;
            break;
        case NAV_COMPOSER:
            g_armed = 1;
            break;
        case NAV_TOOL_APPROVE:
            if (g_tool_pending) {
                PendingTool pt = g_pending_tool;
                g_tool_pending = 0;
                start_tool_job(&pt);
            }
            break;
        case NAV_TOOL_DENY:
            if (g_tool_pending) {
                char note[MSG_LEN];
                snprintf(note, sizeof(note), "[tool denied] %s %s", g_pending_tool.name, g_pending_tool.arg);
                add_and_persist(0, note);
                g_tool_pending = 0;
            }
            break;
        case NAV_MODEL:
            cycle_model();
            break;
        case NAV_STATS: {
            char cmd[2048];
            char session_id[32] = "";
            if (g_session_dir[0]) {
                sscanf(g_session_dir, "%*[^/]/%31s", session_id);
            }
            snprintf(cmd, sizeof(cmd), "setsid nohup bash '%s/open_session_stats.sh' '%s' '%s' >/dev/null 2>&1 &",
                     g_house_root, session_id, g_house_root);
            system(cmd);
            break;
        }
        case NAV_CLOSE:
            g_running = 0;
            break;
    }
}

static void delete_focused_if_session(void) {
    if (g_focus_nav < 1 || g_focus_nav > g_n_nav) return;
    NavItem *it = &g_nav[g_focus_nav - 1];
    if (it->kind != NAV_SESSION) return;
    if (it->session_idx < 0 || it->session_idx >= g_n_sessions) return;
    int was_current = g_session_dir[0] && strcmp(g_sessions[it->session_idx].dir, g_session_dir) == 0;
    delete_session(g_sessions[it->session_idx].dir);
    refresh_sessions();
    if (was_current) new_chat(); /* the deleted session was open - start fresh rather than show a dangling view */
}

/* REAL FIX 2026-08-12 ("hai doesn't have mouse working yet unlike
 * db-hq") - real click-to-select-and-activate, same one-click-does-
 * both shape as db-hq's own handle_click()/hit_test(). Composer is
 * special-cased: a click there should ARM it directly (same as
 * pressing Enter on it), not toggle it off if it was already armed -
 * matches how a text field click behaves everywhere else, not a nav
 * toggle. */
static void handle_mouse_click(int px, int py) {
    for (int i = 0; i < g_n_nav; i++) {
        NavItem *it = &g_nav[i];
        if (it->w <= 0 || it->h <= 0) continue;
        if (px < it->x || px >= it->x + it->w || py < it->y || py >= it->y + it->h) continue;
        g_focus_nav = i + 1;
        if (it->kind == NAV_COMPOSER) { g_armed = 1; return; }
        activate_focused();
        return;
    }
}

static int g_digit_accum = 0; /* multi-digit nav-jump accumulator, house standard (chtpm_parser.c) */

static void handle_key(KeySym ks, char ch) {
    if (g_armed) {
        if (ks == XK_Return || ks == XK_KP_Enter) { submit_composer(); return; }
        if (ks == XK_Escape) { g_armed = 0; return; }
        if (ks == XK_BackSpace) {
            if (g_input_len > 0) { g_input_buf[--g_input_len] = '\0'; }
            return;
        }
        if (ch >= 32 && ch <= 126 && g_input_len < INPUT_BUF_LEN - 1) {
            g_input_buf[g_input_len++] = ch;
            g_input_buf[g_input_len] = '\0';
        }
        return;
    }
    if (ch == 'p') { dump_frame_png(); return; } /* not armed, so 'p' can't collide with composer typing */
    if (ch >= '0' && ch <= '9') {
        /* digit accumulation, ported from the house standard in
         * chtpm_parser.c (~line 2621): greedy multi-digit jump so a
         * single digit still moves focus instantly when unambiguous,
         * but a run of digits (e.g. "1" then "2" for nav item 12)
         * accumulates instead of the first digit always winning.
         * Previous naive version here always let a bare 1-9 digit win
         * outright, so nav items past index 9 were unreachable via
         * relay digit-jump once several sessions existed. */
        int d = ch - '0';
        int new_val = g_digit_accum * 10 + d;
        if (new_val > 0 && new_val <= g_n_nav) {
            g_digit_accum = new_val;
            g_focus_nav = g_digit_accum;
        } else if (d > 0 && d <= g_n_nav) {
            g_digit_accum = d;
            g_focus_nav = g_digit_accum;
        } else {
            g_digit_accum = 0;
        }
        return; /* digit selects (moves focus) only - Enter activates, same convention as everywhere else in this house (direct confirmation 2026-08-12: "i do expect to press enter. (as usual)") */
    }
    if (ks == XK_Up || ks == XK_Left) { if (g_focus_nav > 1) g_focus_nav--; g_digit_accum = 0; return; }
    if (ks == XK_Down || ks == XK_Right) { if (g_focus_nav < g_n_nav) g_focus_nav++; g_digit_accum = 0; return; }
    if (ks == XK_Return || ks == XK_KP_Enter) { g_digit_accum = 0; activate_focused(); return; }
    if (ks == XK_BackSpace) { g_digit_accum = 0; delete_focused_if_session(); return; }
}

/* ---------- agent relay (same shape as db-hq's own, HOUSE_STDS/
 * testing-guide convention): <house_root>/#.desktop/
 * ai_cell_agent_relay.txt, one bare decimal ASCII code per line
 * (digits 48-57, Enter=13, Escape=27, Backspace=8, printable 32-126).
 * This is THE mechanism that makes "you type as human, I inject via
 * relay" (the whole stated point of this GUI) real - dispatched
 * through the SAME handle_key() real KeyPress events use, so relay-
 * driven input and real keyboard input are indistinguishable to the
 * rest of the program. */
static long g_relay_cursor = -1;

static void relay_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/#.desktop/ai_cell_agent_relay.txt", g_house_root);
}

static void dispatch_relay_code(int code) {
    if (code == 13) handle_key(XK_Return, 0);
    else if (code == 27) handle_key(XK_Escape, 0);
    else if (code == 8) handle_key(XK_BackSpace, 0);
    else if (code >= 32 && code <= 126) handle_key(0, (char)code);
}

static int poll_agent_relay(void) {
    char path[PATH_BUF];
    relay_path(path, sizeof(path));
    struct stat stt;
    if (stat(path, &stt) != 0) return 0;
    if (g_relay_cursor < 0) { g_relay_cursor = stt.st_size; return 0; }
    if (stt.st_size < g_relay_cursor) { g_relay_cursor = stt.st_size; return 0; }
    if (stt.st_size == g_relay_cursor) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fseek(f, g_relay_cursor, SEEK_SET);
    char line[32];
    long consumed = g_relay_cursor;
    int n_dispatched = 0;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (!nl) break;
        *nl = '\0';
        long here = ftell(f);
        int code = atoi(line);
        if (code > 0) { dispatch_relay_code(code); n_dispatched++; }
        consumed = here;
    }
    fclose(f);
    g_relay_cursor = consumed;
    return n_dispatched;
}

/* Real single-instance PID tracking + clean SIGTERM shutdown.
 * FOUND LIVE 2026-08-13: repeated test launches left FIVE concurrent
 * khtpm_ai_cell_render processes alive simultaneously, all racing on
 * the same relay file/session dir (root-caused after ~2hrs of
 * "flaky" relay test results that were actually multiple processes
 * fighting over shared state, not a code bug - see
 * _.0.aigent-testing-k9.txt "SCOPE ADDENDUM 2026-08-13" for the full
 * writeup). `pkill -9 khtpm_ai_cell_render` does not reliably match
 * this binary's process given the emoji-laden house-root path in
 * argv - a pidfile + graceful SIGTERM handler + button.sh doing a
 * real pgrep -f kill-before-launch (mirroring
 * *.livedesk-taskbar/ops/run_khtpm_strip.sh's own proven pattern) is
 * the fix, not a bigger hammer. */
static volatile sig_atomic_t g_want_exit = 0;
static void handle_sigterm(int sig) { (void)sig; g_want_exit = 1; }

static void write_pidfile(void) {
    FILE *f = fopen(g_pid_path, "w");
    if (f) { fprintf(f, "%d\n", (int)getpid()); fclose(f); }
}

static void unlink_pidfile(void) {
    remove(g_pid_path);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: khtpm_ai_cell_render.+x <house_root>\n"); return 1; }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_sessions_root, sizeof(g_sessions_root), "%s/&.widgits/ai-cell/sessions", g_house_root);
    snprintf(g_audit_dir, sizeof(g_audit_dir), "%s/%s", g_house_root, AUDIT_DIR_REL);
    snprintf(g_pid_path, sizeof(g_pid_path), "%s/%s", g_house_root, AUDIT_DIR_REL "/ai-cell.pid");
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);
    snprintf(g_emoji_dir, sizeof(g_emoji_dir), "%s/%s", g_house_root, AUDIT_EMOJI_REL);
    mkdir(g_audit_dir, 0755);
    mkdir(g_emoji_dir, 0755);
    write_pidfile();

    XSetErrorHandler(nonfatal_x_error);
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "ai-cell: cannot open display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    vis = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    cmap = XCreateColormap(dpy, RootWindow(dpy, screen), vis, AllocNone);

    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
    swa.background_pixel = 0x141414;
    swa.border_pixel = 0;

    win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)g_win_w, (unsigned)g_win_h,
                         0, depth, InputOutput, vis,
                         CWColormap | CWEventMask | CWBackPixel | CWBorderPixel, &swa);
    XStoreName(dpy, win, "ai-cell");

    /* Managed window + _MOTIF_WM_HINTS, decorations=0 - the real
     * keyboard-focus fix (HOUSE_STDS #21), NOT override_redirect. */
    Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    struct { unsigned long flags, functions, decorations; long input_mode; unsigned long status; } mwm = {2, 0, 0, 0, 0};
    XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)&mwm, 5);

    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);

    XSizeHints *sh = XAllocSizeHints();
    sh->flags = PMinSize;
    sh->min_width = MIN_WIN_W; sh->min_height = MIN_WIN_H;
    XSetWMNormalHints(dpy, win, sh);
    XFree(sh);

    XMapWindow(dpy, win);
    /* Force the position after mapping - window manager may try to remember
     * old position from previous session, so explicitly set it here. */
    XMoveWindow(dpy, win, g_win_x, g_win_y);

    gc = XCreateGC(dpy, win, 0, NULL);
    buf = XCreatePixmap(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h, (unsigned)depth);
    xftdraw_buf = XftDrawCreate(dpy, buf, vis, cmap);

    load_fonts();
    xft_color("#ececec", &col_text);
    xft_color("#a0a0a0", &col_muted);
    xft_color("#22c55e", &col_accent);
    xft_color("#ef4444", &col_danger);
    xft_color("#4d9fff", &col_user);      /* user messages - blue (unused elsewhere in the palette) */
    xft_color("#c084fc", &col_assistant); /* ai responses - purple (unused elsewhere in the palette) */
    xft_color("#9ecbff", &col_user_bright);      /* lighter blue - bullet points under user msgs */
    xft_color("#e2c4ff", &col_assistant_bright); /* lighter purple - bullet points under ai msgs */
    xft_color("#fdfdfd", &col_bullet);           /* lighter white - bullet points, neutral/sys role */
    xft_color("#8a8a8a", &col_subtext);          /* dim grey - italic indented subtext lines */

    g_px_rshift = mask_shift(vis->red_mask);
    g_px_gshift = mask_shift(vis->green_mask);
    g_px_bshift = mask_shift(vis->blue_mask);
    load_emoji_tiles();

    (void)g_backend_mode; /* referenced when BACKEND_AGENT45_LEGACY gets implemented, see file header */
    mkdir(g_sessions_root, 0755);
    refresh_sessions();
    load_model_choice();
    start_new_session();
    add_and_persist(0, "ai-cell v1 — raw Ollama backend, model: stable-code:latest. Real tools: list/read/search run immediately; write/edit/run ask for your approve/deny first. Press composer then Enter to type. History is real and on disk - Backspace on a sidebar row deletes it.");
    refresh_sessions();
    g_focus_nav = 1;

    redraw();

    /* headless verification aid (same convention as db-hq's own):
     * argv[2]=="--dump-and-exit" dumps one frame + receipt and quits
     * immediately - no need for a live human/relay round trip just to
     * prove the window renders. */
    if (argc > 2 && strcmp(argv[2], "--dump-and-exit") == 0) {
        unlink_pidfile(); /* throwaway process - don't leave/clobber a real instance's pidfile */
        dump_frame_png();
        XftDrawDestroy(xftdraw_buf);
        XFreeGC(dpy, gc);
        XFreePixmap(dpy, buf);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
        return 0;
    }

    g_running = 1;
    while (g_running && !g_want_exit) {
        struct timeval tv = {0, 150000};
        fd_set fds; FD_ZERO(&fds); int xfd = ConnectionNumber(dpy); FD_SET(xfd, &fds);
        select(xfd + 1, &fds, NULL, NULL, &tv);

        int need_redraw = g_pending; /* keep polling curl child even with no X events */
        if (poll_agent_relay() > 0) need_redraw = 1;
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            need_redraw = 1;
            if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) { g_running = 0; }
            else if (ev.type == ConfigureNotify) {
                if (ev.xconfigure.width != g_win_w || ev.xconfigure.height != g_win_h) {
                    g_win_w = ev.xconfigure.width; g_win_h = ev.xconfigure.height;
                    XFreePixmap(dpy, buf);
                    buf = XCreatePixmap(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h, (unsigned)depth);
                    XftDrawDestroy(xftdraw_buf);
                    xftdraw_buf = XftDrawCreate(dpy, buf, vis, cmap);
                }
            } else if (ev.type == ButtonPress && ev.xbutton.y < CHROME_H) {
                /* close button lives IN the chrome bar - check it before
                 * falling back to drag, same precedence db-hq's own
                 * handle_click() uses for its synthetic close element. */
                int hit_close = 0;
                for (int i = 0; i < g_n_nav; i++) {
                    if (g_nav[i].kind != NAV_CLOSE) continue;
                    NavItem *it = &g_nav[i];
                    if (ev.xbutton.x >= it->x && ev.xbutton.x < it->x + it->w &&
                        ev.xbutton.y >= it->y && ev.xbutton.y < it->y + it->h) {
                        g_focus_nav = i + 1;
                        activate_focused();
                        hit_close = 1;
                    }
                    break;
                }
                if (!hit_close) {
                    g_dragging = 1; g_drag_start_x = ev.xbutton.x_root; g_drag_start_y = ev.xbutton.y_root;
                    g_drag_win_x0 = g_win_x; g_drag_win_y0 = g_win_y;
                }
            } else if (ev.type == ButtonPress) {
                if (ev.xbutton.x >= g_win_w - RESIZE_GRIP && ev.xbutton.y >= g_win_h - RESIZE_GRIP) {
                    g_resizing = 1; g_resize_start_x = ev.xbutton.x_root; g_resize_start_y = ev.xbutton.y_root;
                    g_resize_w0 = g_win_w; g_resize_h0 = g_win_h;
                } else {
                    handle_mouse_click(ev.xbutton.x, ev.xbutton.y);
                }
            } else if (ev.type == ButtonRelease) {
                g_dragging = 0;
                g_resizing = 0;
            } else if (ev.type == MotionNotify && g_resizing) {
                int nw = g_resize_w0 + (ev.xmotion.x_root - g_resize_start_x);
                int nh = g_resize_h0 + (ev.xmotion.y_root - g_resize_start_y);
                if (nw < MIN_WIN_W) nw = MIN_WIN_W;
                if (nh < MIN_WIN_H) nh = MIN_WIN_H;
                /* ConfigureNotify below owns g_win_w/g_win_h + the pixmap
                 * rebuild (it only rebuilds when the event size differs) */
                XResizeWindow(dpy, win, (unsigned)nw, (unsigned)nh);
            } else if (ev.type == MotionNotify && g_dragging) {
                g_win_x = g_drag_win_x0 + (ev.xmotion.x_root - g_drag_start_x);
                g_win_y = g_drag_win_y0 + (ev.xmotion.y_root - g_drag_start_y);
                XMoveWindow(dpy, win, g_win_x, g_win_y);
            } else if (ev.type == KeyPress) {
                char buf_ch[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, buf_ch, sizeof(buf_ch), &ks, NULL);
                handle_key(ks, n > 0 ? buf_ch[0] : 0);
                /* Escape only disarms the composer now, it does NOT
                 * close the window - real nav-indexed close button is
                 * the standard close mechanism (see draw_close_button()'s
                 * own header comment). */
            }
        }
        if (need_redraw) redraw();
    }

    unlink_pidfile();
    XftDrawDestroy(xftdraw_buf);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, buf);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
