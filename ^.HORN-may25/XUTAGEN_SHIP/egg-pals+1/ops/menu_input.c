/* menu_input - one verb, one binary, no shared headers.
 * Interprets one raw keycode against pieces/system/menu_state.txt
 * (screen, cursor) - moves the cursor on up/down, and on enter, either
 * changes screen or performs an action by shelling out to the relevant
 * single-purpose op (claim_tokens/coin_flip/buy_egg/hatch_egg), capturing
 * its printed message into menu_state.txt's last_message field for
 * compose_menu to display. 'b' is a back-to-main shortcut from any
 * submenu. This op is the router; it does not itself mutate tokens or
 * mint/hatch eggs - those stay in their own ops per doctrine.
 *
 * Usage: menu_input.+x <keycode> [owner_piece_id]
 * owner_piece_id defaults to "user_01" (single-user v1) - prisc+x's
 * generic custom-op dispatch only ever passes ONE argument to a handler
 * (either a register's value or a literal, never both - see
 * exec_custom_op() in prisc+x.c), so the pal script can only hand this
 * op the keycode; a real multi-user version would need a "current user"
 * pointer file read here instead of a hardcoded default.
 *
 * Screens and option counts:
 *   main   (4):            0=User 1=Faucet 2=Store 3=Pets
 *   user   (1):             0=Back
 *   faucet (3):             0=Claim Tokens 1=Coin Flip 2=Back
 *   store  (2):             0=Buy Egg 1=Back
 *   pets   (pet_count + 1): 0..pet_count-1 = one row per owned egg/pet
 *                           (unhatched -> hatch it, hatched -> open its
 *                           GL window), pet_count = Back
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void menu_state_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/system/menu_state.txt", project_root);
}

static void load_menu_state(char *screen, size_t screen_sz, int *cursor, char *msg, size_t msg_sz) {
    strncpy(screen, "main", screen_sz - 1);
    screen[screen_sz - 1] = '\0';
    *cursor = 0;
    msg[0] = '\0';

    char path[PATH_BUF];
    menu_state_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;
        if (strcmp(key, "screen") == 0) { strncpy(screen, val, screen_sz - 1); screen[screen_sz - 1] = '\0'; }
        else if (strcmp(key, "cursor") == 0) *cursor = atoi(val);
        else if (strcmp(key, "last_message") == 0) { strncpy(msg, val, msg_sz - 1); msg[msg_sz - 1] = '\0'; }
    }
    fclose(f);
}

static void save_menu_state(const char *screen, int cursor, const char *msg) {
    char path[PATH_BUF];
    menu_state_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "screen=%s\n", screen);
    fprintf(f, "cursor=%d\n", cursor);
    fprintf(f, "last_message=%s\n", msg);
    fclose(f);
}

static void inventory_path(const char *owner_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/world_01/map_lobby/%s/inventory.txt", project_root, owner_id);
}

static int count_pets(const char *owner_id) {
    char path[PATH_BUF];
    inventory_path(owner_id, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0]) n++;
    }
    fclose(f);
    return n;
}

static int get_pet_id_at(const char *owner_id, int index, char *out, size_t out_sz) {
    char path[PATH_BUF];
    inventory_path(owner_id, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int i = 0;
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (!line[0]) continue;
        if (i == index) {
            /* Piece ids are genuinely short ("egg_1") despite line[] being
             * declared MAX_LINE; same class of warning fixed in
             * mutaclsym/system/prisc+x.c and generate_egg.c - suppressed
             * narrowly rather than widening out[] to match line[]'s size. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", line);
#pragma GCC diagnostic pop
            found = 1;
            break;
        }
        i++;
    }
    fclose(f);
    return found;
}

static int read_piece_int(const char *piece_id, const char *key, int def) {
    char path[PATH_BUF + 32];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, piece_id);
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static int option_count(const char *screen, const char *owner_id) {
    if (strcmp(screen, "main") == 0) return 4;
    if (strcmp(screen, "faucet") == 0) return 3;
    if (strcmp(screen, "store") == 0) return 2;
    if (strcmp(screen, "pets") == 0) return count_pets(owner_id) + 1; /* + Back */
    return 1; /* user: just Back */
}

static void run_action_op(const char *op_name, const char *arg, char *msg_out, size_t msg_sz) {
    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/%s.+x' '%s'", project_root, op_name, arg);
    FILE *pf = popen(cmd, "r");
    msg_out[0] = '\0';
    if (!pf) { snprintf(msg_out, msg_sz, "Action failed to start."); return; }
    if (!fgets(msg_out, msg_sz, pf)) snprintf(msg_out, msg_sz, "Action produced no output.");
    pclose(pf);
    msg_out[strcspn(msg_out, "\n")] = '\0';
}

/* Opens a pet's GL window as an independent, long-running process - not
 * captured/waited on, unlike run_action_op's short-lived ops. Once this
 * (short-lived) menu_input process exits, the window reparents to init
 * and keeps running on its own; no double-fork needed for that. */
static void spawn_egg_window(const char *pet_id, char *msg_out, size_t msg_sz) {
    char window_path[PATH_BUF];
    snprintf(window_path, sizeof(window_path), "%s/system/egg_window", project_root);

    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); close(devnull); }
        execl(window_path, window_path, pet_id, (char *)NULL);
        _exit(127);
    }
    if (pid > 0) snprintf(msg_out, msg_sz, "Opened %s in a new window.", pet_id);
    else snprintf(msg_out, msg_sz, "Could not open window for %s.", pet_id);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <keycode> [owner_piece_id]\n", argv[0]);
        return 1;
    }
    int key = atoi(argv[1]);
    const char *owner_id = (argc >= 3) ? argv[2] : "user_01";
    resolve_root();

    char screen[32];
    int cursor;
    char msg[256];
    load_menu_state(screen, sizeof(screen), &cursor, msg, sizeof(msg));

    int count = option_count(screen, owner_id);
    if (count < 1) count = 1;

    if (key == 'w' || key == ARROW_UP) {
        cursor = (cursor - 1 + count) % count;
        msg[0] = '\0';
    } else if (key == 's' || key == ARROW_DOWN) {
        cursor = (cursor + 1) % count;
        msg[0] = '\0';
    } else if (key == 'b') {
        strcpy(screen, "main");
        cursor = 0;
        msg[0] = '\0';
    } else if (key == 10 || key == 13) {
        if (strcmp(screen, "main") == 0) {
            const char *targets[] = {"user", "faucet", "store", "pets"};
            strcpy(screen, targets[cursor]);
            cursor = 0;
            msg[0] = '\0';
        } else if (strcmp(screen, "faucet") == 0) {
            if (cursor == 0) run_action_op("claim_tokens", owner_id, msg, sizeof(msg));
            else if (cursor == 1) run_action_op("coin_flip", owner_id, msg, sizeof(msg));
            else { strcpy(screen, "main"); cursor = 0; msg[0] = '\0'; }
        } else if (strcmp(screen, "store") == 0) {
            if (cursor == 0) run_action_op("buy_egg", owner_id, msg, sizeof(msg));
            else { strcpy(screen, "main"); cursor = 0; msg[0] = '\0'; }
        } else if (strcmp(screen, "pets") == 0) {
            int pet_count = count_pets(owner_id);
            if (cursor < pet_count) {
                char pet_id[64];
                if (get_pet_id_at(owner_id, cursor, pet_id, sizeof(pet_id))) {
                    if (read_piece_int(pet_id, "hatched", 0)) {
                        spawn_egg_window(pet_id, msg, sizeof(msg));
                    } else {
                        run_action_op("hatch_egg", pet_id, msg, sizeof(msg));
                    }
                }
            } else {
                strcpy(screen, "main");
                cursor = 0;
                msg[0] = '\0';
            }
        } else {
            /* user: only option is Back */
            strcpy(screen, "main");
            cursor = 0;
            msg[0] = '\0';
        }
    }

    save_menu_state(screen, cursor, msg);
    return 0;
}
