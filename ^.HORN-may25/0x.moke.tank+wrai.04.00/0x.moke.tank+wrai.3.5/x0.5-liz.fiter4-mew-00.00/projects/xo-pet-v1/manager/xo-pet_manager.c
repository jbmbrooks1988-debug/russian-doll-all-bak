#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <sys/wait.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_ENTITIES 64

// xo-pet-v1 Manager (Simplified PoC)
char project_root[MAX_PATH] = ".";
char active_target_id[64] = "xelector";
int sim_active = 0;
char last_response[256] = "System Initialized.";

// Simulation Paths (Relative to project_root)
const char* world_path = "pieces/world_tank_01/map_enclosure";
const char* state_path_world = "pieces/world_tank_01/state.txt";
const char* log_dir = "pieces/world_tank_01/logs";

void resolve_paths(const char* argv0) {
    if (!getcwd(project_root, sizeof(project_root))) strcpy(project_root, ".");
    FILE *kvp = fopen("location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                strncpy(project_root, line + 13, MAX_PATH - 1);
                project_root[strcspn(project_root, "\n\r")] = 0;
            }
        }
        fclose(kvp);
    }
    printf("[Manager] Project Root: %s\n", project_root);
}

// --- Simulation Logic (Adopted from Moke-Pet) ---

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
        time_t now; time(&now); char* ts = ctime(&now); ts[strlen(ts)-1] = '\0';
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
            fclose(f); return 1;
        }
    }
    fclose(f); return 0;
}

void resolve_op_path(const char* entity, const char* method, char* out_path) {
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

void dispatch_sim_op(const char* entity, const char* method, const char* arg1, const char* arg2) {
    char op_path[MAX_PATH] = {0};
    resolve_op_path(entity, method, op_path);
    if (strlen(op_path) == 0) return;

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

void run_simulation_epoch() {
    int epoch = get_epoch();
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s/%s", project_root, world_path);
    DIR* d = opendir(search_path);
    if (!d) return;

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
        
        // Skip user-controlled lizard (they act via buttons)
        if (strcmp(liz, active_target_id) == 0) {
            dispatch_sim_op(liz, "breathe", NULL, NULL);
            dispatch_sim_op(liz, "check_death", NULL, NULL);
            continue;
        }

        dispatch_sim_op(liz, "breathe", NULL, NULL);
        dispatch_sim_op(liz, "scan", NULL, NULL);
        
        char obs_path[MAX_PATH];
        snprintf(obs_path, MAX_PATH, "%s/%s/%s/memory/observations.txt", project_root, world_path, liz);
        FILE* f = fopen(obs_path, "r");
        if (!f) {
            dispatch_sim_op(liz, "rest", NULL, NULL);
            log_event(epoch, liz, "rest", NULL);
            dispatch_sim_op(liz, "check_death", NULL, NULL);
            continue;
        }

        char target_food[64] = "";
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, f)) {
            char* pipe = strchr(line, '|');
            if (pipe) {
                *pipe = '\0';
                char* id = line; char* type = pipe + 2; type[strcspn(type, "\n\r")] = 0;
                if (strstr(type, "food")) {
                    char* id_trim = id; while(*id_trim == ' ') id_trim++;
                    char* space = strchr(id_trim, ' '); if (space) *space = '\0';
                    strncpy(target_food, id_trim, 63); break;
                }
            }
        }
        fclose(f);

        if (strlen(target_food) > 0) {
            dispatch_sim_op(liz, "eat", target_food, NULL);
            log_event(epoch, liz, "eat", target_food);
        } else {
            dispatch_sim_op(liz, "rest", NULL, NULL);
            log_event(epoch, liz, "rest", NULL);
        }
        dispatch_sim_op(liz, "check_death", NULL, NULL);
    }

    char epoch_log[MAX_PATH];
    snprintf(epoch_log, MAX_PATH, "pieces/world_tank_01/logs/epoch_%d.txt", epoch);
    for (int i = 0; i < lizard_count; i++) {
        dispatch_sim_op(lizard_ids[i], "train", epoch_log, lizard_ids[i]);
    }
    increment_epoch(epoch);
    snprintf(last_response, sizeof(last_response), "Epoch %d Complete.", epoch);
}

// --- GUI & Input Logic ---

void write_gui_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/manager/gui_state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "app_title=XO-PET V1 (PoC)\n");
        fprintf(f, "active_target=%s\n", active_target_id);
        fprintf(f, "sim_status=%s\n", sim_active ? "RUNNING" : "PAUSED");
        fprintf(f, "last_response=%s\n", last_response);
        fprintf(f, "epoch=%d\n", get_epoch());
        
        if (strcmp(active_target_id, "xelector") != 0) {
            char stats_path[MAX_PATH];
            snprintf(stats_path, sizeof(stats_path), "%s/%s/%s/memory/stats.txt", project_root, world_path, active_target_id);
            FILE *sf = fopen(stats_path, "r");
            if (sf) {
                char line[256];
                while (fgets(line, sizeof(line), sf)) {
                    if (strncmp(line, "hp=", 3) == 0) fprintf(f, "pet_hp=%s", line + 3);
                    if (strncmp(line, "hunger=", 7) == 0) fprintf(f, "pet_hunger=%s", line + 7);
                }
                fclose(sf);
            }
        }

        char pdl_cmd[MAX_PATH];
        if (strcmp(active_target_id, "xelector") == 0) {
            snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/../../../pieces/system/pdl/+x/pdl_reader.+x' %s/projects/xo-pet-v1/pieces/xelector list_methods", project_root, project_root);
        } else {
             snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/../../../pieces/system/pdl/+x/pdl_reader.+x' %s/%s/%s list_methods", project_root, project_root, world_path, active_target_id);
        }

        FILE *pf = popen(pdl_cmd, "r");
        if (pf) {
            char line[256]; int idx = 2; fprintf(f, "piece_methods=");
            while (fgets(line, sizeof(line), pf)) {
                line[strcspn(line, "\n\r")] = 0;
                fprintf(f, "<button label=\"%s\" onClick=\"KEY:%d\" /> ", line, idx++);
            }
            fprintf(f, "\n"); pclose(pf);
        }
        fclose(f);
    }
}

void check_state_updates() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/manager/active_target.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        fscanf(f, "%63s", active_target_id); fclose(f); remove(path);
        printf("[Manager] Switched target to: %s\n", active_target_id);
    }

    snprintf(path, sizeof(path), "%s/manager/sim_control.txt", project_root);
    f = fopen(path, "r");
    if (f) {
        fscanf(f, "%d", &sim_active); fclose(f); remove(path);
        printf("[Manager] Sim Status: %s\n", sim_active ? "START" : "PAUSE");
    }
}

void route_input(int key) {
    if (key == '9' || key == 2009) {
        if (sim_active) run_simulation_epoch();
        else snprintf(last_response, sizeof(last_response), "Sim is PAUSED. Cannot end turn.");
    }
    
    if (key >= '2' && key <= '8') {
        int idx = key - '0'; char method_name[64] = ""; char pdl_cmd[MAX_PATH];
        if (strcmp(active_target_id, "xelector") == 0) {
            snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/../../../pieces/system/pdl/+x/pdl_reader.+x' %s/projects/xo-pet-v1/pieces/xelector list_methods", project_root, project_root);
        } else {
            snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/../../../pieces/system/pdl/+x/pdl_reader.+x' %s/%s/%s list_methods", project_root, project_root, world_path, active_target_id);
        }
        FILE *pf = popen(pdl_cmd, "r");
        if (pf) {
            char line[256]; int current = 2;
            while (fgets(line, sizeof(line), pf)) {
                if (current == idx) { strncpy(method_name, line, 63); method_name[strcspn(method_name, "\n\r")] = 0; break; }
                current++;
            }
            pclose(pf);
        }

        if (strlen(method_name) > 0) {
            printf("[Manager] Dispatching: %s\n", method_name);
            if (strcmp(active_target_id, "xelector") == 0) {
                snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/../../../pieces/system/pdl/+x/pdl_reader.+x' %s/projects/xo-pet-v1/pieces/xelector get_method %s", project_root, project_root, method_name);
            } else {
                snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/../../../pieces/system/pdl/+x/pdl_reader.+x' %s/%s/%s get_method %s", project_root, project_root, world_path, active_target_id, method_name);
            }
            pf = popen(pdl_cmd, "r");
            if (pf) {
                char op_path[MAX_PATH];
                if (fgets(op_path, sizeof(op_path), pf)) {
                    op_path[strcspn(op_path, "\n\r")] = 0;
                    if (strcmp(op_path, "NOT_FOUND") != 0) {
                        char full_op[MAX_PATH*2];
                        if (op_path[0] == '/') snprintf(full_op, sizeof(full_op), "'%s'", op_path);
                        else snprintf(full_op, sizeof(full_op), "'%s/%s'", project_root, op_path);
                        system(full_op);
                        snprintf(last_response, sizeof(last_response), "Executed %s.", method_name);
                    }
                }
                pclose(pf);
            }
        }
    }
    check_state_updates();
    write_gui_state();
}

void* input_thread(void* arg) {
    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/../../../pieces/keyboard/history.txt", project_root);
    struct stat st; long last_pos = 0;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;
    while (1) {
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *f = fopen(hist_path, "r");
                if (f) {
                    fseek(f, last_pos, SEEK_SET); int key;
                    while (fscanf(f, "[%*[^]]] KEY_PRESSED: %d\n", &key) == 1) route_input(key);
                    last_pos = ftell(f); fclose(f);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(100000);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    resolve_paths(argv[0]);
    write_gui_state();
    pthread_t t;
    pthread_create(&t, NULL, input_thread, NULL);
    printf("=== XO-PET V1 Manager Active ===\n");
    while (1) sleep(1);
    return 0;
}
