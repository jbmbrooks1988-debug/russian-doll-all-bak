#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <time.h>

#define MAX_PATH 1024
#define MAX_LINE 256
#define MAX_ENTITIES 64

char project_root[MAX_PATH] = ".";
const char* world_path = "pieces/world_tank_01/map_enclosure";
const char* state_path_world = "pieces/world_tank_01/state.txt";
const char* log_dir = "pieces/world_tank_01/logs";

void resolve_root(const char* argv0) {
    char path[MAX_PATH];
    strncpy(path, argv0, MAX_PATH);
    char* last_slash = strrchr(path, '/');
    if (last_slash) {
        *last_slash = '\0';
        strncpy(project_root, path, MAX_PATH);
    } else {
        strcpy(project_root, ".");
    }
    printf("[Manager] Resolved Root: %s\n", project_root);
}

typedef struct {
    char id[64];
    char type[64];
} Entity;

int get_epoch() {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s", project_root, state_path_world);
    FILE* f = fopen(full_path, "r");
    if (!f) return 1;
    char line[MAX_LINE];
    int epoch = 1;
    while (fgets(line, MAX_LINE, f)) {
        if (strncmp(line, "epoch=", 6) == 0) {
            epoch = atoi(line + 6);
            break;
        }
    }
    fclose(f);
    return epoch;
}

void increment_epoch(int current) {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s", project_root, state_path_world);
    FILE* f = fopen(full_path, "w");
    if (f) {
        fprintf(f, "epoch=%d\nstatus=active\n", current + 1);
        fclose(f);
    }
}

void log_event(int epoch, const char* entity, const char* action, const char* target) {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s/epoch_%d.txt", project_root, log_dir, epoch);
    FILE* f = fopen(full_path, "a");
    if (f) {
        time_t now;
        time(&now);
        char* ts = ctime(&now);
        ts[strlen(ts)-1] = '\0'; // remove newline
        fprintf(f, "[%s] %s | %s | %s\n", ts, entity, action, target ? target : "NONE");
        fclose(f);
    }
}

int get_type(const char* id, char* type) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s/%s/%s/state.txt", project_root, world_path, id);
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, f)) {
        if (strncmp(line, "type | ", 7) == 0) {
            strncpy(type, line + 7, 63);
            type[strcspn(type, "\n\r")] = 0;
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

void resolve_op(const char* entity, const char* method, char* out_path) {
    char pdl_path[MAX_PATH];
    snprintf(pdl_path, MAX_PATH, "%s/%s/%s/piece.pdl", project_root, world_path, entity);
    FILE* f = fopen(pdl_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, f)) {
        if (strstr(line, method)) {
            char* pipe2 = strrchr(line, '|');
            if (pipe2) {
                strncpy(out_path, pipe2 + 2, MAX_PATH);
                out_path[strcspn(out_path, "\n\r")] = 0;
                fclose(f); return;
            }
        }
    }
    fclose(f);
}

void dispatch(const char* entity, const char* method, const char* arg1, const char* arg2) {
    char op_path[MAX_PATH] = {0};
    resolve_op(entity, method, op_path);
    if (strlen(op_path) == 0) return;

    printf("[Manager] %s acts: %s(%s, %s)\n", entity, method, arg1 ? arg1 : "", arg2 ? arg2 : "");
    pid_t pid = fork();
    if (pid == 0) {
        chdir(project_root);
        if (arg1 && arg2) execl(op_path, op_path, arg1, arg2, NULL);
        else if (arg1) execl(op_path, op_path, arg1, NULL);
        else execl(op_path, op_path, NULL);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

int main(int argc, char* argv[]) {
    resolve_root(argv[0]);
    int epoch = get_epoch();
    printf("=== Epoch %d ===\n", epoch);

    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s/%s", project_root, world_path);
    DIR* d = opendir(search_path);
    if (!d) return 1;

    char lizard_ids[MAX_ENTITIES][64];
    int lizard_count = 0;
    struct dirent* entry;
    while ((entry = readdir(d)) && lizard_count < MAX_ENTITIES) {
        if (entry->d_name[0] == '.') continue;
        char type[64];
        if (get_type(entry->d_name, type) && strcmp(type, "lizard") == 0) {
            strncpy(lizard_ids[lizard_count++], entry->d_name, 63);
        }
    }
    closedir(d);

    for (int i = 0; i < lizard_count; i++) {
        const char* liz = lizard_ids[i];
        printf("\n--- Turn: %s ---\n", liz);
        
        // 0. Metabolism Phase
        dispatch(liz, "breathe", NULL, NULL);

        // 1. Perception Phase: Run 'scan' Op
        dispatch(liz, "scan", NULL, NULL);
        
        char obs_path[MAX_PATH];
        snprintf(obs_path, MAX_PATH, "%s/%s/%s/memory/observations.txt", project_root, world_path, liz);
        FILE* f = fopen(obs_path, "r");
        if (!f) {
            dispatch(liz, "rest", NULL, NULL);
            log_event(epoch, liz, "rest", NULL);
            dispatch(liz, "check_death", NULL, NULL);
            continue;
        }

        char target_food[64] = "";
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, f)) {
            char* pipe = strchr(line, '|');
            if (pipe) {
                *pipe = '\0';
                char* id = line;
                char* type = pipe + 2;
                type[strcspn(type, "\n\r")] = 0;
                
                if (strstr(type, "food")) {
                    char* id_trim = id;
                    while(*id_trim == ' ') id_trim++;
                    char* space = strchr(id_trim, ' ');
                    if (space) *space = '\0';
                    strncpy(target_food, id_trim, 63);
                    break;
                }
            }
        }
        fclose(f);

        if (strlen(target_food) > 0) {
            dispatch(liz, "eat", target_food, NULL);
            log_event(epoch, liz, "eat", target_food);
        } else {
            dispatch(liz, "rest", NULL, NULL);
            log_event(epoch, liz, "rest", NULL);
        }

        // 3. Mortality Phase
        dispatch(liz, "check_death", NULL, NULL);
    }

    // 4. Perception-to-Training Loop
    printf("\n--- Training Phase ---\n");
    char epoch_log[MAX_PATH];
    snprintf(epoch_log, MAX_PATH, "%s/epoch_%d.txt", log_dir, epoch);
    for (int i = 0; i < lizard_count; i++) {
        dispatch(lizard_ids[i], "train", epoch_log, lizard_ids[i]);
    }

    increment_epoch(epoch);
    printf("\n=== Epoch %d Complete ===\n", epoch);
    return 0;
}
