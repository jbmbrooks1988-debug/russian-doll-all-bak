/*
 * pal_editor_module.c - PAL Script Editor for op-ed
 * 
 * Responsibilities:
 * 1. Track which piece and event is being edited
 * 2. Maintain list of PAL instructions
 * 3. Show available Ops in a palette
 * 4. Allow adding/deleting instructions
 * 5. Save script as .asm file
 * 6. Bind script to piece's piece.pdl METHOD entry
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_INSTRUCTIONS 64
#define MAX_OPS 32

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "op-ed";
char active_piece[MAX_LINE] = "xlector";
char active_event[MAX_LINE] = "on_interact";

/* UI State */
int gui_focus_index = 1;
int selected_op_idx = 0;

/* PAL instruction storage */
typedef struct {
    char mnemonic[32];
    char args[256];
} Instruction;

Instruction instructions[MAX_INSTRUCTIONS];
int instruction_count = 0;
int cursor_line = 0;

/* Available Ops */
typedef struct {
    char name[32];
    char syntax[128];
    char desc[128];
} OpDef;

OpDef available_ops[MAX_OPS] = {
    {"addi", "addi rD, rS, imm", "Add immediate value"},
    {"beq", "beq rS, rT, label", "Branch if equal"},
    {"lw", "lw rD, addr(rS)", "Load word from memory"},
    {"sw", "sw rS, addr(rD)", "Store word to memory"},
    {"j", "j label", "Jump to label"},
    {"jalr", "jalr handler", "Jump and link register"},
    {"halt", "halt", "Stop execution"},
    {"sleep", "sleep ms", "Pause execution"},
    {"read_history", "read_history rD", "Read input history"},
    {"read_state", "read_state rD, piece, key", "Read piece state"},
    {"read_pos", "read_pos rD, rE, piece", "Read piece position"},
    {"OP", "OP module::op args", "Call custom op"},
    /* High-Level Blocks (Exploratory) */
    {"Show Text", "exec echo msg > last_resp", "RPG: Display message"},
    {"Change Gold", "OP user::mod_gold n", "RPG: Adjust currency"},
    {"Control Vars", "addi r1, r0, val", "RPG: Set variable"},
    {"Move 10 Steps", "OP playrm::move n", "Scratch: Motion"},
    {"Go to XY", "OP playrm::jump x y", "Scratch: Placement"},
    {"Wait 1 Sec", "sleep 1000", "Scratch: Timing"},
    {"Switch Costume", "OP piece::set_icon i", "Scratch: Appearance"},
    {"Broadcast", "exec echo msg > pulse", "Scratch: Events"},
    {"Touching?", "read_state r1, p, dist", "Scratch: Sensing"},
    {"Forever", "j start", "Scratch: Control Loop"},
};
int op_count = 22;

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

void sync_focus() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/active_gui_index.txt", project_root) != -1) {
        FILE *f = fopen(path, "r");
        if (f) { 
            int active_idx = 0; 
            if (fscanf(f, "%d", &active_idx) == 1) { 
                if (active_idx > 0) gui_focus_index = active_idx; 
            } 
            fclose(f); 
        }
        free(path);
    }
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

void load_script() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/events/%s.asm",
                 project_root, current_project, active_piece, active_event) == -1) return;
    
    FILE *f = fopen(path, "r");
    instruction_count = 0;
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f) && instruction_count < MAX_INSTRUCTIONS) {
            char *trimmed = trim_str(line);
            if (trimmed[0] == '\0' || trimmed[0] == '#') continue;
            
            char *space = strchr(trimmed, ' ');
            if (space) {
                int len = space - trimmed;
                if (len >= 32) len = 31;
                strncpy(instructions[instruction_count].mnemonic, trimmed, len);
                instructions[instruction_count].mnemonic[len] = '\0';
                strncpy(instructions[instruction_count].args, trim_str(space + 1), 255);
            } else {
                strncpy(instructions[instruction_count].mnemonic, trimmed, 31);
                instructions[instruction_count].mnemonic[31] = '\0';
                instructions[instruction_count].args[0] = '\0';
            }
            instruction_count++;
        }
        fclose(f);
    }
    free(path);
}

void save_script() {
    char *piece_path = NULL;
    if (asprintf(&piece_path, "%s/projects/%s/pieces/%s", 
                 project_root, current_project, active_piece) != -1) {
        mkdir(piece_path, 0777);
        free(piece_path);
    }
    
    char *dir_path = NULL;
    if (asprintf(&dir_path, "%s/projects/%s/pieces/%s/events",
                 project_root, current_project, active_piece) != -1) {
        mkdir(dir_path, 0777);
        free(dir_path);
    }
    
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/events/%s.asm",
                 project_root, current_project, active_piece, active_event) == -1) return;
    
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "# PAL Script: %s/%s\n", active_piece, active_event);
        fprintf(f, "# Generated by PAL Editor\n\n");
        for (int i = 0; i < instruction_count; i++) {
            if (instructions[i].args[0] != '\0') {
                fprintf(f, "%s %s\n", instructions[i].mnemonic, instructions[i].args);
            } else {
                fprintf(f, "%s\n", instructions[i].mnemonic);
            }
        }
        fclose(f);
        
        char *pdl_path = NULL;
        if (asprintf(&pdl_path, "%s/projects/%s/pieces/%s/piece.pdl",
                     project_root, current_project, active_piece) != -1) {
            FILE *pdl = fopen(pdl_path, "a");
            if (pdl) {
                fprintf(pdl, "METHOD       | %s | events/%s.asm\n", active_event, active_event);
                fclose(pdl);
            }
            free(pdl_path);
        }
    }
    free(path);
}

void set_response(const char* msg) {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/op-ed/manager/response.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) { fprintf(f, "%-57s", msg); fclose(f); }
        free(path);
    }
}

void write_editor_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (!f) { free(path); return; }
        
        fprintf(f, "project_id=%s\n", current_project);
        fprintf(f, "gui_focus=%d\n", gui_focus_index);
        fprintf(f, "active_target_id=%s\n", active_piece);
        fprintf(f, "pal_editor_piece=%s\n", active_piece);
        fprintf(f, "pal_editor_event=%s\n", active_event);
        fprintf(f, "pal_editor_cursor=%d\n", cursor_line);
        fprintf(f, "pal_editor_instr_count=%d\n", instruction_count);
        
        char instr_display[MAX_LINE] = "";
        for (int i = 0; i < instruction_count && i < 10; i++) {
            char marker = (i == cursor_line) ? '>' : ' ';
            char line_buf[128];
            if (instructions[i].args[0] != '\0') {
                snprintf(line_buf, sizeof(line_buf), "║  %c %2d: %-8s %s\n", marker, i, instructions[i].mnemonic, instructions[i].args);
            } else {
                snprintf(line_buf, sizeof(line_buf), "║  %c %2d: %-8s\n", marker, i, instructions[i].mnemonic);
            }
            strncat(instr_display, line_buf, sizeof(instr_display) - strlen(instr_display) - 1);
        }
        fprintf(f, "instruction_list=%s\n", instr_display);
        
        char op_display[MAX_LINE] = "";
        for (int i = 0; i < op_count && i < 22; i++) {
            char line_buf[64];
            char marker_l = (i == selected_op_idx) ? '[' : ' ';
            char marker_r = (i == selected_op_idx) ? ']' : ' ';
            snprintf(line_buf, sizeof(line_buf), "%c%d:%s%c ", marker_l, i, available_ops[i].name, marker_r);
            strncat(op_display, line_buf, sizeof(op_display) - strlen(op_display) - 1);
        }
        fprintf(f, "op_palette=%s\n", op_display);
        
        fprintf(f, "r0=0\n");
        fprintf(f, "r1=0\n");
        fprintf(f, "r2=0\n");
        fprintf(f, "r3=0\n");
        fprintf(f, "r4=0\n");
        fprintf(f, "r5=0\n");
        fprintf(f, "r6=0\n");
        fprintf(f, "r7=0\n");
        
        char resp[MAX_LINE] = "";
        char *resp_path = NULL;
        if (asprintf(&resp_path, "%s/projects/op-ed/manager/response.txt", project_root) != -1) {
            FILE *rf = fopen(resp_path, "r");
            if (rf) { fgets(resp, sizeof(resp), rf); fclose(rf); }
            free(resp_path);
        }
        fprintf(f, "last_response=%s\n", resp);
        
        fprintf(f, "piece_methods=║  [1-9] Select Op  [A]dd  [D]el  [S]ave  [ESC]Back  ║\n");
        
        fclose(f);
    }
    free(path);
}

int is_active_layout() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0; }
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) {
        fclose(f);
        int res = (strstr(line, "pal_editor.chtpm") != NULL);
        free(path);
        return res;
    }
    fclose(f);
    free(path);
    return 0;
}

int main() {
    resolve_paths();
    
    char *mgr_state = NULL;
    if (asprintf(&mgr_state, "%s/projects/op-ed/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(mgr_state, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = trim_str(line);
                    char *v = trim_str(eq + 1);
                    if (strcmp(k, "project_id") == 0) strncpy(current_project, v, MAX_LINE - 1);
                    if (strcmp(k, "pal_editor_piece") == 0) strncpy(active_piece, v, MAX_LINE - 1);
                    else if (strcmp(k, "active_target_id") == 0 && strlen(active_piece) == 0) strncpy(active_piece, v, MAX_LINE - 1);
                    if (strcmp(k, "pal_editor_event") == 0) strncpy(active_event, v, MAX_LINE - 1);
                }
            }
            fclose(f);
        }
        free(mgr_state);
    }
    
    load_script();
    write_editor_state();
    
    char *pulse = NULL;
    if (asprintf(&pulse, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(pulse, "a");
        if (f) { fprintf(f, "P\n"); fclose(f); }
        free(pulse);
    }
    
    long last_pos = 0;
    struct stat st;
    char *history_path = NULL;
    if (asprintf(&history_path, "%s/pieces/apps/player_app/history.txt", project_root) == -1) return 1;
    
    if (stat(history_path, &st) == 0) last_pos = st.st_size;
    
    while (1) {
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }
        
        sync_focus();
        
        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    int key;
                    while (fscanf(hf, "%d", &key) == 1) {
                        /* Global hotkeys */
                        if (key == 27) {
                            set_response("Returned to op-ed");
                            char *layout_path = NULL;
                            if (asprintf(&layout_path, "%s/pieces/display/layout_changed.txt", project_root) != -1) {
                                FILE *lf = fopen(layout_path, "a");
                                if (lf) {
                                    fprintf(lf, "projects/op-ed/layouts/op-ed.chtpm\n");
                                    fclose(lf);
                                }
                                free(layout_path);
                            }
                            continue;
                        }

                        if (gui_focus_index == 1) { // INSTRUCTIONS
                            if (key == 119 || key == 1002 || key == 'w') {
                                if (cursor_line > 0) cursor_line--;
                            } else if (key == 115 || key == 1003 || key == 's') {
                                if (cursor_line < instruction_count - 1) cursor_line++;
                            } else if (key == 100 || key == 'd') { // DEL
                                if (cursor_line < instruction_count) {
                                    for (int i = cursor_line; i < instruction_count - 1; i++) {
                                        instructions[i] = instructions[i+1];
                                    }
                                    instruction_count--;
                                    if (cursor_line >= instruction_count) cursor_line = instruction_count - 1;
                                    if (cursor_line < 0) cursor_line = 0;
                                }
                            }
                        } else if (gui_focus_index == 2) { // OPS
                            if (key == 1000 || key == 'a') {
                                if (selected_op_idx > 0) selected_op_idx--;
                                else selected_op_idx = op_count - 1;
                            } else if (key == 1001 || key == 'd') {
                                if (selected_op_idx < op_count - 1) selected_op_idx++;
                                else selected_op_idx = 0;
                            } else if (key == 10 || key == 13) { // ENTER: Add selected op
                                if (instruction_count < MAX_INSTRUCTIONS) {
                                    strncpy(instructions[instruction_count].mnemonic, available_ops[selected_op_idx].name, 31);
                                    instructions[instruction_count].args[0] = '\0';
                                    instruction_count++;
                                    cursor_line = instruction_count - 1;
                                    set_response("Added instruction");
                                }
                            }
                        }

                        /* Legacy hotkeys for convenience */
                        if (key == 115) { // 's' (Save)
                            save_script();
                            char msg[MAX_LINE];
                            snprintf(msg, sizeof(msg), "Saved %s/%s.asm", active_piece, active_event);
                            set_response(msg);
                        } else if (key == 97) { // 'a' (Add from OPS)
                            if (instruction_count < MAX_INSTRUCTIONS) {
                                strncpy(instructions[instruction_count].mnemonic, available_ops[selected_op_idx].name, 31);
                                instructions[instruction_count].args[0] = '\0';
                                instruction_count++;
                                cursor_line = instruction_count - 1;
                            }
                        }
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            }
        }
        
        write_editor_state();
        usleep(16667);
    }
    
    free(history_path);
    return 0;
}
