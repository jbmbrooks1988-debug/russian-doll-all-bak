#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>

void handle_sigint(int sig) {
    _exit(130);  /* Immediate exit, no cleanup */
}

#define MAX_INST 1024
#define MAX_LABELS 128
#define MAX_VARS 256
#define MAX_OPS 64
#define MAX_INCLUDES 16
#define MEM_SIZE 4096
#define STRING_POOL_START 0xF00

typedef enum { OP_ADDI, OP_BEQ, OP_LW, OP_SW, OP_JALR, OP_J, OP_HALT, OP_CUSTOM } OpBase;

typedef struct {
    OpBase op;
    int rd, rs1, rs2, imm;
    char label_ref[32];
    char custom_name[32];
} Inst;

typedef struct {
    char name[32];
    int addr;
} Label;

typedef struct {
    char name[32];
    int addr;
    char type[16];
    int value;
    int size;
} Variable;

typedef struct {
    char name[32];
    char type[16];
    char handler[64];
    char desc[128];
} CustomOp;

int32_t regs[16] = {0};
int32_t mem[MEM_SIZE] = {0};
Inst program[MAX_INST];
Label labels[MAX_LABELS];
Variable variables[MAX_VARS];
CustomOp custom_ops[MAX_OPS];
int label_count = 0, inst_count = 0, var_count = 0, op_count = 0;
int next_var_addr = 0;
char included_files[MAX_INCLUDES][64];
int include_count = 0;

void trim(char *str) {
    char *start = str, *end;
    while (isspace(*start)) start++;
    end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    end[1] = '\0';
    memmove(str, start, strlen(start) + 1);
}

int is_included(const char *path) {
    for (int i = 0; i < include_count; i++)
        if (strcmp(included_files[i], path) == 0) return 1;
    return 0;
}

void add_variable(const char *name, const char *type, const char *value_str) {
    if (var_count >= MAX_VARS) return;
    Variable *v = &variables[var_count++];
    strncpy(v->name, name, 31);
    strncpy(v->type, type, 15);
    v->addr = next_var_addr;
    
    if (strcmp(type, "int") == 0) {
        v->value = atoi(value_str);
        v->size = 1;
        mem[next_var_addr++] = v->value;
    } else if (strcmp(type, "char") == 0 || strcmp(type, "string") == 0) {
        char *p = strchr(value_str, '"');
        if (p) {
            p++;
            char *end = strchr(p, '"');
            if (end) *end = '\0';
            v->value = STRING_POOL_START + var_count;
            v->size = 1;
            mem[STRING_POOL_START + var_count] = (int32_t)(uintptr_t)p;
            next_var_addr = STRING_POOL_START + 1;
        }
    } else if (strcmp(type, "array") == 0) {
        v->size = 0;
        char *copy = strdup(value_str);
        char *tok = strtok(copy, ",");
        while (tok && v->size < 16) {
            mem[next_var_addr + v->size] = atoi(tok);
            v->size++;
            tok = strtok(NULL, ",");
        }
        free(copy);
        next_var_addr += v->size;
    }
}

int find_variable(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(variables[i].name, name) == 0) return i;
    return -1;
}

void add_custom_op(const char *name, const char *type, const char *handler, const char *desc) {
    if (op_count >= MAX_OPS) return;
    CustomOp *c = &custom_ops[op_count++];
    strncpy(c->name, name, 31);
    strncpy(c->type, type, 15);
    strncpy(c->handler, handler, 63);
    strncpy(c->desc, desc, 127);
}

int find_custom_op(const char *name) {
    for (int i = 0; i < op_count; i++)
        if (strcmp(custom_ops[i].name, name) == 0) return i;
    return -1;
}

void parse_ops_file(const char *path) {
    if (is_included(path)) return;
    strncpy(included_files[include_count++], path, 63);
    
    FILE *f = fopen(path, "r");
    if (!f) return;
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '#' || line[0] == '\0') continue;
        
        if (strncmp(line, "#include", 8) == 0) {
            char inc_path[64];
            if (sscanf(line + 8, " \"%63[^\"]\"", inc_path) == 1) {
                parse_ops_file(inc_path);
            }
            continue;
        }
        
        char name[32], type[16], handler[64], desc[128] = "";
        char *brace = strchr(line, '{');
        if (brace) {
            *brace = '\0';
            strncpy(desc, brace + 1, 126);
            char *end = strchr(desc, '}');
            if (end) *end = '\0';
        }
        
        if (sscanf(line, "%31s %15s %63s", name, type, handler) >= 3) {
            add_custom_op(name, type, handler, desc);
        }
    }
    fclose(f);
}

int parse_vars_line(char *line) {
    trim(line);
    if (line[0] == '#' || line[0] == '\0') return 0;
    if (strncmp(line, "#include", 8) == 0) return 0;
    
    char name[32], type[16], value[128];
    if (sscanf(line, "%31s %15s %127[^\n]", name, type, value) == 3) {
        add_variable(name, type, value);
        return 1;
    }
    return 0;
}

int is_variable_line(char *line) {
    char trimmed[128];
    strncpy(trimmed, line, 127);
    trim(trimmed);
    if (trimmed[0] == '#' || trimmed[0] == '\0') return 0;
    if (strncmp(trimmed, "#include", 8) == 0) return 0;
    
    char name[32], type[16], value[128];
    if (sscanf(trimmed, "%31s %15s %127[^\n]", name, type, value) == 3) {
        if (strcmp(type, "int") == 0 || strcmp(type, "char") == 0 || 
            strcmp(type, "string") == 0 || strcmp(type, "array") == 0) {
            return 1;
        }
    }
    return 0;
}

void parse_line(char *line, int pass) {
    char original[128];
    strncpy(original, line, 127);
    trim(line);
    
    if (line[0] == '#' || line[0] == '\0') return;
    if (strncmp(line, "#include", 8) == 0) return;
    if (is_variable_line(line)) return;
    
    char part[32], *p = line;
    if (sscanf(p, "%s", part) != 1) return;
    
    if (part[strlen(part) - 1] == ':') {
        if (pass == 1) {
            part[strlen(part) - 1] = '\0';
            strcpy(labels[label_count].name, part);
            labels[label_count++].addr = inst_count;
        }
        return;
    }
    
    if (pass == 2) {
        Inst *i = &program[inst_count];
        memset(i, 0, sizeof(Inst));
        
        int op_idx = find_custom_op(part);
        if (op_idx >= 0) {
            i->op = OP_CUSTOM;
            strcpy(i->custom_name, part);
            
            /* Parse arguments: layout_op x2, x1 (rd, menu_id) or move_player x1 */
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            
            char arg_copy[64];
            strncpy(arg_copy, args, 63);
            arg_copy[63] = '\0';
            
            /* Check for comma-separated args (rd, rs1) */
            char *comma = strchr(arg_copy, ',');
            if (comma && strcmp(i->custom_name, "layout_op") == 0) {
                /* layout_op rd, menu_id */
                *comma = '\0';
                trim(arg_copy);
                int reg;
                if (sscanf(arg_copy, "x%d", &reg) == 1) {
                    i->rd = reg;
                }
                
                /* Parse second arg (menu_id) */
                char *arg2 = comma + 1;
                while (*arg2 == ' ' || *arg2 == '\t') arg2++;
                char arg2_copy[64];
                strncpy(arg2_copy, arg2, 63);
                char *c = strchr(arg2_copy, ' ');
                if (c) *c = '\0';
                c = strchr(arg2_copy, '\n');
                if (c) *c = '\0';
                trim(arg2_copy);
                
                if (sscanf(arg2_copy, "x%d", &reg) == 1) {
                    i->rs1 = reg;
                }
            } else {
                /* Single arg: move_player x1 */
                char *c = strchr(arg_copy, ',');
                if (c) *c = '\0';
                c = strchr(arg_copy, ' ');
                if (c) *c = '\0';
                c = strchr(arg_copy, '\n');
                if (c) *c = '\0';
                c = strchr(arg_copy, '\r');
                if (c) *c = '\0';
                trim(arg_copy);
                
                int reg;
                if (sscanf(arg_copy, "x%d", &reg) == 1) {
                    i->rs1 = reg;
                }
            }
        } else if (strcmp(part, "addi") == 0) {
            sscanf(line, "%*s x%d, x%d, %d", &i->rd, &i->rs1, &i->imm);
            i->op = OP_ADDI;
        } else if (strcmp(part, "beq") == 0) {
            sscanf(line, "%*s x%d, x%d, %s", &i->rs1, &i->rs2, i->label_ref);
            i->op = OP_BEQ;
        } else if (strcmp(part, "lw") == 0) {
            sscanf(line, "%*s x%d, %d(x%d)", &i->rd, &i->imm, &i->rs1);
            i->op = OP_LW;
        } else if (strcmp(part, "sw") == 0) {
            sscanf(line, "%*s x%d, %d(x%d)", &i->rs2, &i->imm, &i->rs1);
            i->op = OP_SW;
        } else if (strcmp(part, "jalr") == 0) {
            sscanf(line, "%*s x%d, x%d, %d", &i->rd, &i->rs1, &i->imm);
            i->op = OP_JALR;
        } else if (strcmp(part, "j") == 0) {
            sscanf(line, "%*s %s", i->label_ref);
            i->op = OP_J;
        } else if (strcmp(part, "halt") == 0) {
            i->op = OP_HALT;
        }
    }
    inst_count++;
}

void load_mem(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    int addr, val;
    while (fscanf(f, "%d %d", &addr, &val) == 2)
        if (addr >= 0 && addr < MEM_SIZE) mem[addr] = val;
    fclose(f);
}

void save_mem(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < MEM_SIZE; i++)
        if (mem[i] != 0) fprintf(f, "%d %d\n", i, mem[i]);
    fclose(f);
}

void exec_custom_op(Inst *i) {
    int op_idx = find_custom_op(i->custom_name);
    if (op_idx < 0) return;
    
    CustomOp *op = &custom_ops[op_idx];
    
    if (strncmp(op->handler, "+x/", 3) == 0) {
        char cmd[512];
        char *ext = strstr(op->handler, ".+x");
        if (ext) {
            char script_path[64];
            strncpy(script_path, op->handler, ext - op->handler + 3);
            script_path[ext - op->handler + 3] = '\0';
            
            /* For layout_op, use system() for interactive stdin */
            if (strcmp(i->custom_name, "layout_op") == 0) {
                /* Pass menu_id from rs1, result goes to shared file */
                snprintf(cmd, sizeof(cmd), "./%s %d > /tmp/prisc_layout_result.txt",
                         script_path, regs[i->rs1]);
                system(cmd);
                
                /* Read result from shared file */
                FILE *rf = fopen("/tmp/prisc_layout_result.txt", "r");
                if (rf) {
                    char result[16];
                    if (fgets(result, sizeof(result), rf)) {
                        regs[i->rd] = atoi(result);
                    }
                    fclose(rf);
                }
            } else {
                /* Pass rs1 as the argument (single register for array pointer) */
                snprintf(cmd, sizeof(cmd), "./%s %d",
                         script_path, regs[i->rs1]);
                
                FILE *pipe = popen(cmd, "r");
                if (pipe) {
                    char result[256];
                    /* Print all output from custom op */
                    while (fgets(result, sizeof(result), pipe)) {
                        printf("%s", result);
                    }
                    pclose(pipe);
                }
            }
        }
    } else if (strcmp(op->handler, "builtin_out") == 0) {
        printf("OUT: %d\n", regs[i->rs1]);
    } else if (strcmp(op->handler, "builtin_halt") == 0) {
    }
}

int main(int argc, char **argv) {
    if (argc < 2) return printf("Usage: %s <prog> [mem_in] [mem_out] [ops_file]\n", argv[0]), 1;
    
    /* Set up Ctrl+C handler */
    signal(SIGINT, handle_sigint);
    
    parse_ops_file("default_op.txt");
    if (argc > 4) parse_ops_file(argv[4]);
    
    FILE *f = fopen(argv[1], "r");
    if (!f) return printf("Error: Could not open %s\n", argv[1]), 1;
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char trimmed[256];
        strncpy(trimmed, line, 255);
        trim(trimmed);
        if (is_variable_line(trimmed)) {
            char name[32], type[16], value[256];
            if (sscanf(trimmed, "%31s %15s %255[^\n]", name, type, value) == 3) {
                add_variable(name, type, value);
            }
        }
    }
    
    rewind(f);
    inst_count = 0;
    
    while (fgets(line, sizeof(line), f)) parse_line(line, 1);
    rewind(f);
    inst_count = 0;
    
    while (fgets(line, sizeof(line), f)) parse_line(line, 2);
    fclose(f);
    
    if (argc > 2) load_mem(argv[2]);
    
    int pc = 0;
    while (pc < inst_count) {
        Inst i = program[pc];
        regs[0] = 0;
        int next_pc = pc + 1;
        
        if (i.op == OP_CUSTOM) {
            exec_custom_op(&i);
            if (find_custom_op(i.custom_name) >= 0 &&
                strcmp(custom_ops[find_custom_op(i.custom_name)].handler, "builtin_halt") == 0) {
                break;
            }
        } else {
            switch (i.op) {
                case OP_ADDI: regs[i.rd] = regs[i.rs1] + i.imm; break;
                case OP_BEQ: {
                    int target = -1;
                    for (int l = 0; l < label_count; l++)
                        if (strcmp(labels[l].name, i.label_ref) == 0) target = labels[l].addr;
                    if (regs[i.rs1] == regs[i.rs2]) next_pc = target;
                    break;
                }
                case OP_J: {
                    int target = -1;
                    for (int l = 0; l < label_count; l++)
                        if (strcmp(labels[l].name, i.label_ref) == 0) target = labels[l].addr;
                    next_pc = target;
                    break;
                }
                case OP_LW:  regs[i.rd] = mem[(regs[i.rs1] + i.imm) % MEM_SIZE]; break;
                case OP_SW:  mem[(regs[i.rs1] + i.imm) % MEM_SIZE] = regs[i.rs2]; break;
                case OP_JALR: { int tmp = pc + 1; next_pc = regs[i.rs1] + i.imm; regs[i.rd] = tmp; break; }
                case OP_HALT: pc = inst_count; continue;
                default: break;
            }
        }
        pc = next_pc;
    }
    
    if (argc > 3) save_mem(argv[3]);
    return 0;
}
