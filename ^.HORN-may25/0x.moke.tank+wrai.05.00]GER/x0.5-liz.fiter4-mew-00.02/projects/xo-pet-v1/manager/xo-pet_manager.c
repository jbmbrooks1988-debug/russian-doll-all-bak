#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_ENTITIES 64
#define MAX_VAR_VALUE 65536

// TPM XO-PET Manager (Canonical Fix)
char system_root[MAX_PATH] = ".";
char project_id[64] = "xo-pet-v1";
char active_target_id[64] = "xelector";
int sim_active = 0;
char last_response[256] = "System Initialized.";

// Simulation Paths (Relative to system_root)
char world_path[MAX_PATH];
char state_path_world[MAX_PATH];
char log_dir[MAX_PATH];

char* trim_str(char *str) {
    char *end;
    if(!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    if (!getcwd(system_root, sizeof(system_root))) strcpy(system_root, ".");
    
    // Resolve via location_kvp if available (Standard TPMOS)
    // Check multiple relative paths based on common execution contexts
    char *kvp_attempts[] = {
        "pieces/locations/location_kvp",
        "../pieces/locations/location_kvp",
        "../../pieces/locations/location_kvp",
        "../../../pieces/locations/location_kvp"
    };
    
    FILE *kvp = NULL;
    for (int i = 0; i < 4; i++) {
        kvp = fopen(kvp_attempts[i], "r");
        if (kvp) break;
    }

    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "project_root") == 0) {
                    strncpy(system_root, trim_str(eq + 1), MAX_PATH - 1);
                }
            }
        }
        fclose(kvp);
    }
    
    // Construct internal paths (Canonical TPMOS Project Structure)
    snprintf(world_path, MAX_PATH, "projects/xo-pet-v1/pieces/world_tank_01/map_enclosure");
    snprintf(state_path_world, MAX_PATH, "projects/xo-pet-v1/pieces/world_tank_01/state.txt");
    snprintf(log_dir, MAX_PATH, "projects/xo-pet-v1/pieces/world_tank_01/logs");
    
    printf("[Manager] System Root: %s\n", system_root);
}

// --- Simulation Logic ---

int get_epoch() {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s", system_root, state_path_world);
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
    snprintf(full_path, MAX_PATH, "%s/%s", system_root, state_path_world);
    FILE* f = fopen(full_path, "w");
    if (f) {
        fprintf(f, "epoch=%d\nstatus=active\n", current + 1);
        fclose(f);
    }
}

void log_event(int epoch, const char* entity, const char* action, const char* target) {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s/epoch_%d.txt", system_root, log_dir, epoch);
    FILE* f = fopen(full_path, "a");
    if (f) {
        time_t now; time(&now); char* ts = ctime(&now); ts[strlen(ts)-1] = '\0';
        fprintf(f, "[%s] %s | %s | %s\n", ts, entity, action, target ? target : "NONE");
        fclose(f);
    }
}

int get_type(const char* id, char* type) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s/%s/%s/state.txt", system_root, world_path, id);
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
    snprintf(pdl_path, MAX_PATH, "%s/%s/%s/piece.pdl", system_root, world_path, entity);
    FILE* f = fopen(pdl_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, f)) {
        if (strstr(line, method)) {
            char* pipe2 = strrchr(line, '|');
            if (pipe2) {
                strncpy(out_path, trim_str(pipe2 + 1), MAX_PATH);
                fclose(f); return;
            }
        }
    }
    fclose(f);
}

void dispatch_sim_op(const char* entity, const char* method, const char* arg1, const char* arg2) {
    char op_rel_path[MAX_PATH] = {0};
    resolve_op_path(entity, method, op_rel_path);
    if (strlen(op_rel_path) == 0) return;

    pid_t pid = fork();
    if (pid == 0) {
        char full_op[MAX_PATH];
        snprintf(full_op, MAX_PATH, "%s/%s", system_root, op_rel_path);
        execl(full_op, full_op, arg1, arg2, NULL);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

void run_simulation_epoch() {
    int epoch = get_epoch();
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s/%s", system_root, world_path);
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
        if (strcmp(liz, active_target_id) == 0) {
            dispatch_sim_op(liz, "breathe", NULL, NULL);
            dispatch_sim_op(liz, "check_death", NULL, NULL);
            continue;
        }
        dispatch_sim_op(liz, "breathe", NULL, NULL);
        dispatch_sim_op(liz, "scan", NULL, NULL);
        
        char obs_path[MAX_PATH];
        snprintf(obs_path, MAX_PATH, "%s/%s/%s/memory/observations.txt", system_root, world_path, liz);
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
                char* id = trim_str(line);
                char* type = trim_str(pipe + 1);
                if (strstr(type, "food")) {
                    strncpy(target_food, id, 63); break;
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
    snprintf(epoch_log, MAX_PATH, "projects/xo-pet-v1/pieces/world_tank_01/logs/epoch_%d.txt", epoch);
    for (int i = 0; i < lizard_count; i++) {
        dispatch_sim_op(lizard_ids[i], "train", epoch_log, lizard_ids[i]);
    }
    increment_epoch(epoch);
    snprintf(last_response, sizeof(last_response), "Epoch %d Complete.", epoch);
}

// --- GUI & Input Logic ---

void write_gui_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/xo-pet-v1/manager/gui_state.txt", system_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "module_path=projects/xo-pet-v1/manager/+x/xo-pet-v1_manager.+x\n");
        fprintf(f, "app_title=XO-PET V1 (PoC)\n");
        fprintf(f, "active_target=%s\n", active_target_id);
        fprintf(f, "sim_status=%s\n", sim_active ? "RUNNING" : "PAUSED");
        fprintf(f, "last_response=%s\n", last_response);
        fprintf(f, "epoch=%d\n", get_epoch());
        
        if (strcmp(active_target_id, "xelector") != 0) {
            char stats_path[MAX_PATH];
            snprintf(stats_path, sizeof(stats_path), "%s/%s/%s/memory/stats.txt", system_root, world_path, active_target_id);
            FILE *sf = fopen(stats_path, "r");
            if (sf) {
                char line[256];
                while (fgets(line, MAX_LINE, sf)) {
                    if (strncmp(line, "hp=", 3) == 0) fprintf(f, "pet_hp=%s", trim_str(line + 3));
                    if (strncmp(line, "hunger=", 7) == 0) fprintf(f, "pet_hunger=%s", trim_str(line + 7));
                }
                fclose(sf);
            }
        } else {
             fprintf(f, "pet_hp=N/A\npet_hunger=N/A\n");
        }

        // 1. Build Pet List
        char pet_list[MAX_VAR_VALUE] = "";
        char search_path[MAX_PATH];
        snprintf(search_path, MAX_PATH, "%s/%s", system_root, world_path);
        DIR* d = opendir(search_path);
        if (d) {
            struct dirent* entry;
            while ((entry = readdir(d))) {
                if (entry->d_name[0] == '.') continue;
                char type[64];
                if (get_type(entry->d_name, type) && strcmp(type, "lizard") == 0) {
                    char btn[256];
                    snprintf(btn, sizeof(btn), "<button label=\"Possess %s\" onClick=\"SET_POSSESS:%s\" /> ", entry->d_name, entry->d_name);
                    strcat(pet_list, btn);
                }
            }
            closedir(d);
        }
        fprintf(f, "pet_list=%s\n", pet_list);

        // 2. Build Methods
        char pdl_cmd[MAX_PATH];
        if (strcmp(active_target_id, "xelector") == 0) {
            snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/pieces/system/pdl/+x/pdl_reader.+x' %s/projects/xo-pet-v1/pieces/xelector list_methods", system_root, system_root);
        } else {
             snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/pieces/system/pdl/+x/pdl_reader.+x' %s/%s/%s list_methods", system_root, system_root, world_path, active_target_id);
        }

        FILE *pf = popen(pdl_cmd, "r");
        if (pf) {
            char line[256]; int idx = 2; fprintf(f, "piece_methods=");
            while (fgets(line, sizeof(line), pf)) {
                char *m = trim_str(line);
                if (strncmp(m, "possess_", 8) == 0) continue;
                fprintf(f, "<button label=\"%s\" onClick=\"KEY:%d\" /> ", m, idx++);
            }
            fprintf(f, "\n"); pclose(pf);
        }
        fclose(f);
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
            snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/pieces/system/pdl/+x/pdl_reader.+x' %s/projects/xo-pet-v1/pieces/xelector list_methods", system_root, system_root);
        } else {
            snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/pieces/system/pdl/+x/pdl_reader.+x' %s/%s/%s list_methods", system_root, system_root, world_path, active_target_id);
        }
        FILE *pf = popen(pdl_cmd, "r");
        if (pf) {
            char line[256]; int current = 2;
            while (fgets(line, sizeof(line), pf)) {
                if (current == idx) { strncpy(method_name, trim_str(line), 63); break; }
                current++;
            }
            pclose(pf);
        }

        if (strlen(method_name) > 0) {
            char pdl_cmd_get[MAX_PATH];
            if (strcmp(active_target_id, "xelector") == 0) {
                snprintf(pdl_cmd_get, sizeof(pdl_cmd_get), "'%s/pieces/system/pdl/+x/pdl_reader.+x' %s/projects/xo-pet-v1/pieces/xelector get_method %s", system_root, system_root, method_name);
            } else {
                snprintf(pdl_cmd_get, sizeof(pdl_cmd_get), "'%s/pieces/system/pdl/+x/pdl_reader.+x' %s/%s/%s get_method %s", system_root, system_root, world_path, active_target_id, method_name);
            }
            pf = popen(pdl_cmd_get, "r");
            if (pf) {
                char op_rel_path[MAX_PATH];
                if (fgets(op_rel_path, sizeof(op_rel_path), pf)) {
                    char *p = trim_str(op_rel_path);
                    if (strcmp(p, "NOT_FOUND") != 0) {
                        char full_op[MAX_PATH*2];
                        snprintf(full_op, sizeof(full_op), "%s/%s", system_root, p);
                        system(full_op);
                        snprintf(last_response, sizeof(last_response), "Executed %s.", method_name);
                    }
                }
                pclose(pf);
            }
        }
    }
    
    // Check internal state updates from Ops
    char pos_path[MAX_PATH], sim_path[MAX_PATH];
    snprintf(pos_path, MAX_PATH, "%s/projects/xo-pet-v1/manager/active_target.txt", system_root);
    FILE *f = fopen(pos_path, "r");
    if (f) { fscanf(f, "%63s", active_target_id); fclose(f); remove(pos_path); }

    snprintf(sim_path, MAX_PATH, "%s/projects/xo-pet-v1/manager/sim_control.txt", system_root);
    f = fopen(sim_path, "r");
    if (f) { fscanf(f, "%d", &sim_active); fclose(f); remove(sim_path); }

    write_gui_state();
}

void* input_thread(void* arg) {
    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/keyboard/history.txt", system_root);
    struct stat st; long last_pos = 0;
    while (1) {
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *f = fopen(hist_path, "r");
                if (f) {
                    fseek(f, last_pos, SEEK_SET); char line[MAX_LINE];
                    while (fgets(line, MAX_LINE, f)) {
                         if (strstr(line, "KEY_PRESSED:")) {
                             int key;
                             if (sscanf(line, "[%*[^]]] KEY_PRESSED: %d", &key) == 1) route_input(key);
                         } else if (strstr(line, "COMMAND: SET_POSSESS:")) {
                             char *val = strstr(line, "SET_POSSESS:") + 12;
                             strncpy(active_target_id, trim_str(val), 63);
                             snprintf(last_response, sizeof(last_response), "Possessed %s.", active_target_id);
                             write_gui_state();
                         }
                    }
                    last_pos = ftell(f); fclose(f);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    resolve_paths();
    write_gui_state();
    pthread_t t;
    pthread_create(&t, NULL, input_thread, NULL);
    printf("=== XO-PET V1 Manager Active ===\n");
    while (1) sleep(1);
    return 0;
}
