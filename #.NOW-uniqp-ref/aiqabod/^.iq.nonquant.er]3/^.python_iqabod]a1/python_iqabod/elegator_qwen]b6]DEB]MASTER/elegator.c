#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>

#define MAX_CMD_LEN 1024
#define MAX_PATH_LEN 512
#define MAX_PROMPT_LEN 4096

// Structure to hold elegator state
typedef struct {
    char project_path[MAX_PATH_LEN];
    char project_task[MAX_PROMPT_LEN];
    char elegator_prompt[MAX_PROMPT_LEN];
    int agent_count;
    int max_agents;
    char backup_dir[MAX_PATH_LEN];
    char progress_file[MAX_PATH_LEN];
} ElegatorState;

// Function prototypes
void print_usage();
int initialize_elegator(ElegatorState *state, char *project_path, char *task);
int create_backup_dir(ElegatorState *state);
int create_progress_file(ElegatorState *state);
int run_qwen_agent(ElegatorState *state, int agent_num, char *prompt);
int check_agent_status(int pid);
int backup_current_state(ElegatorState *state);
int log_progress(ElegatorState *state, char *message);
int cleanup_old_agents(ElegatorState *state);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    ElegatorState state;
    state.agent_count = 0;
    state.max_agents = 3; // Maximum 3 agents as specified
    
    // Initialize the elegator with project path and task
    if (initialize_elegator(&state, argv[1], argv[2]) != 0) {
        fprintf(stderr, "Failed to initialize elegator\n");
        return 1;
    }

    printf("Elegator initialized for project: %s\n", state.project_path);
    printf("Task: %s\n", state.project_task);
    
    // Log the start of the elegator
    log_progress(&state, "Elegator started with task");

    // Main loop - run until tokens expire or task is complete
    int continue_running = 1;
    int current_agent = 0;
    
    while (continue_running && state.agent_count < state.max_agents) {
        printf("\n--- Starting agent %d ---\n", current_agent);
        
        // Create prompt for the agent
        char agent_prompt[MAX_PROMPT_LEN];
        snprintf(agent_prompt, sizeof(agent_prompt), 
                "%s\n\nProject Context: %s\nCurrent Task: %s\n\nThis is agent %d. Continue the work from where the previous agent left off, or start the task if this is the first agent. Report your progress and any decisions made.", 
                state.elegator_prompt, state.project_path, state.project_task, current_agent);
        
        // Run the qwen agent
        int agent_pid = run_qwen_agent(&state, current_agent, agent_prompt);
        
        if (agent_pid > 0) {
            printf("Started agent %d with PID %d\n", current_agent, agent_pid);
            state.agent_count++;
            
            // Wait for the agent to complete or fail
            int status;
            pid_t result = waitpid(agent_pid, &status, 0);
            
            if (result == -1) {
                perror("waitpid failed");
                log_progress(&state, "Agent failed unexpectedly");
            } else {
                if (WIFEXITED(status)) {
                    printf("Agent %d completed with exit code %d\n", current_agent, WEXITSTATUS(status));
                    
                    // Check if the agent thinks it should continue
                    // For now, we'll continue until max agents reached
                    if (WEXITSTATUS(status) == 0) {
                        log_progress(&state, "Agent completed successfully");
                    } else {
                        log_progress(&state, "Agent completed with errors");
                    }
                } else if (WIFSIGNALED(status)) {
                    printf("Agent %d was terminated by signal %d\n", current_agent, WTERMSIG(status));
                    log_progress(&state, "Agent was terminated");
                }
            }
            
            // Check if we should continue based on agent's output or other criteria
            // For now, we'll continue until max agents or user stops us
            printf("Agent %d finished. Continue? (y/n): ");
            char response[10];
            if (fgets(response, sizeof(response), stdin) == NULL || 
                (response[0] != 'y' && response[0] != 'Y')) {
                continue_running = 0;
                log_progress(&state, "User chose to stop");
            }
        } else {
            printf("Failed to start agent %d\n", current_agent);
            log_progress(&state, "Failed to start agent");
            break;
        }
        
        current_agent++;
    }
    
    printf("\nElegator finished. Total agents run: %d\n", state.agent_count);
    log_progress(&state, "Elegator finished");
    
    return 0;
}

void print_usage() {
    printf("Usage: elegator <project_path> <task_description>\n");
    printf("Example: elegator ./gitlet.c]j0009 \"Implement version control system\"\n");
}

int initialize_elegator(ElegatorState *state, char *project_path, char *task) {
    // Set up project path
    strncpy(state->project_path, project_path, MAX_PATH_LEN - 1);
    state->project_path[MAX_PATH_LEN - 1] = '\0';

    // Set up task
    strncpy(state->project_task, task, MAX_PROMPT_LEN - 1);
    state->project_task[MAX_PROMPT_LEN - 1] = '\0';

    // Read the template prompt file
    FILE *prompt_file = fopen("elegator_template_prompt.txt", "r");
    if (prompt_file != NULL) {
        size_t bytes_read = fread(state->elegator_prompt, 1, MAX_PROMPT_LEN - 1, prompt_file);
        state->elegator_prompt[bytes_read] = '\0';
        fclose(prompt_file);

        // Replace placeholders in the template
        // For simplicity in C, we'll do a basic replacement
        // In a more complex implementation, we'd want a proper template engine
        char temp_prompt[MAX_PROMPT_LEN * 2];
        snprintf(temp_prompt, sizeof(temp_prompt),
                "You are an AI coding assistant called 'the_elegator'. Your role is to manage and coordinate AI agents to complete complex programming tasks. Follow these guidelines:\n\n"
                "## Core Responsibilities:\n"
                "1. Analyze the project structure and current state\n"
                "2. Break down the main task into manageable subtasks\n"
                "3. Start specialized agents ('employolo_#') for specific tasks\n"
                "4. Review the work of each agent\n"
                "5. Decide whether to continue with more agents or terminate\n"
                "6. Maintain project state and progress tracking\n\n"
                "## Project Context:\n"
                "- Project directory: %s\n"
                "- Main task: %s\n"
                "- Current working directory: %s\n\n"
                "## Agent Management:\n"
                "- Start agents with specific focused tasks\n"
                "- Limit to maximum 3 concurrent agents\n"
                "- Monitor agent progress and output\n"
                "- If an agent gets stuck or the context becomes corrupted, start a fresh agent with summarized context\n"
                "- Keep numbered backup files of important states\n"
                "- Maintain progress_reports.txt for handoffs and restarts\n\n"
                "## Output Requirements:\n"
                "- Report what each agent accomplished\n"
                "- Note any issues or blockers encountered\n"
                "- Suggest next steps or improvements\n"
                "- If starting a new agent, clearly define its specific task\n\n"
                "## Backup and Progress Tracking:\n"
                "- Create backups before major changes\n"
                "- Log all significant actions in progress_reports.txt\n"
                "- Number backup files chronologically\n"
                "- If restarting, summarize previous progress for the new agent\n\n"
                "Begin by analyzing the current project state and proposing a plan for completing the main task.",
                state->project_path, state->project_task, state->project_path);

        strncpy(state->elegator_prompt, temp_prompt, MAX_PROMPT_LEN - 1);
        state->elegator_prompt[MAX_PROMPT_LEN - 1] = '\0';
    } else {
        // Fallback to default prompt if template file is not found
        snprintf(state->elegator_prompt, MAX_PROMPT_LEN,
            "You are an AI coding assistant called 'the_elegator'. Your role is to manage and coordinate AI agents to complete complex programming tasks. Follow these guidelines:\n\n"
            "1. Analyze the project structure and current state\n"
            "2. Break down the main task into manageable subtasks\n"
            "3. Start specialized agents ('employolo_#') for specific tasks\n"
            "4. Review the work of each agent\n"
            "5. Decide whether to continue with more agents or terminate\n"
            "6. Maintain project state and progress tracking\n\n"
            "Project directory: %s\n"
            "Main task: %s\n\n"
            "Begin by analyzing the current project state and proposing a plan for completing the main task.",
            state->project_path, state->project_task);
    }

    // Set up backup directory
    snprintf(state->backup_dir, MAX_PATH_LEN, "%s/backup", state->project_path);

    // Set up progress file
    snprintf(state->progress_file, MAX_PATH_LEN, "%s/progress_reports.txt", state->project_path);

    // Create backup directory if it doesn't exist
    if (create_backup_dir(state) != 0) {
        fprintf(stderr, "Warning: Could not create backup directory\n");
    }

    // Create progress file if it doesn't exist
    if (create_progress_file(state) != 0) {
        fprintf(stderr, "Warning: Could not create progress file\n");
    }

    return 0;
}

int create_backup_dir(ElegatorState *state) {
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", state->backup_dir);
    
    int result = system(cmd);
    if (result == -1) {
        perror("system() failed");
        return -1;
    }
    
    return 0;
}

int create_progress_file(ElegatorState *state) {
    FILE *file = fopen(state->progress_file, "a");
    if (file == NULL) {
        perror("fopen failed");
        return -1;
    }
    
    // Write header if file is new/empty
    fseek(file, 0, SEEK_END);
    if (ftell(file) == 0) {
        fprintf(file, "# Progress Report for Elegator\n");
        fprintf(file, "Project: %s\n", state->project_path);
        fprintf(file, "Task: %s\n", state->project_task);
        fprintf(file, "Started: %ld\n", time(NULL));
        fprintf(file, "---\n");
    }
    
    fclose(file);
    return 0;
}

int run_qwen_agent(ElegatorState *state, int agent_num, char *prompt) {
    // Create a temporary file for the prompt
    char prompt_file[MAX_PATH_LEN];
    snprintf(prompt_file, sizeof(prompt_file), "%s/agent_%d_prompt.txt", state->backup_dir, agent_num);

    FILE *pf = fopen(prompt_file, "w");
    if (pf == NULL) {
        perror("fopen prompt file failed");
        return -1;
    }

    fprintf(pf, "%s", prompt);
    fclose(pf);

    // Build command to run qwen with the prompt
    // For this implementation, we'll run the command synchronously to properly track it
    // Using -p flag for prompt and -y for auto-approval (yolo mode)
    // Since we can't use command substitution, we'll pass the prompt content directly
    // by reading it back and escaping quotes properly
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd),
        "cd \"%s\" && qwen -p \"@%s\" -y > \"agent_%d_output.txt\" 2>&1",
        state->project_path, prompt_file, agent_num);

    printf("Running command: %s\n", cmd);

    // Execute the command and return the exit status
    int result = system(cmd);
    if (result == -1) {
        perror("system() failed");
        return -1;
    }

    // Return the exit status of the command
    return WEXITSTATUS(result);
}

int check_agent_status(int pid) {
    // Check if process is still running
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);
    
    if (result == 0) {
        // Process is still running
        return 1;
    } else if (result == pid) {
        // Process has finished
        return 0;
    } else {
        // Error occurred
        return -1;
    }
}

int backup_current_state(ElegatorState *state) {
    char timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));
    
    char backup_cmd[MAX_CMD_LEN];
    snprintf(backup_cmd, sizeof(backup_cmd), 
        "cp -r \"%s\" \"%s/gitlet_backup_%s\"", 
        state->project_path, state->backup_dir, timestamp);
    
    printf("Creating backup: %s\n", backup_cmd);
    
    int result = system(backup_cmd);
    if (result == -1) {
        perror("system() failed");
        return -1;
    }
    
    return 0;
}

int log_progress(ElegatorState *state, char *message) {
    FILE *file = fopen(state->progress_file, "a");
    if (file == NULL) {
        perror("fopen progress file failed");
        return -1;
    }
    
    time_t now = time(NULL);
    fprintf(file, "[%ld] %s\n", now, message);
    
    fclose(file);
    return 0;
}

int cleanup_old_agents(ElegatorState *state) {
    // In a real implementation, this would kill any remaining agent processes
    // For now, we'll just return success
    return 0;
}