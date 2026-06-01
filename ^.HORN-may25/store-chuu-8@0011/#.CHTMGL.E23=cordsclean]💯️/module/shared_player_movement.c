#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <time.h>
#include <errno.h>

// Shared memory block structure (matching the one in the main application)
typedef struct {
    int module_id;
    int active;                    // Whether this module is active
    int command_ready;             // Flag: 1 if command is ready for module
    int response_ready;            // Flag: 1 if response is ready for main
    char command_buffer[4096];     // Command from main to module
    char response_buffer[4096];    // Response from module to main
    int last_command_timestamp;    // For detecting stale commands
    int last_response_timestamp;   // For detecting stale responses
    int sem_id;                    // Semaphore ID for synchronization
} ModuleSharedMem;

// Player position variables (global for persistence)
float player_x = 0.0f;
float player_y = 0.0f;
float player_z = 0.0f;

// Semaphore operations union
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Function to generate the same key as used in the main application
key_t generate_shm_key(int module_idx) {
    char pathname[256];
    snprintf(pathname, sizeof(pathname), "/tmp/chtml_shmem_%d", module_idx);
    
    // Create the file if it doesn't exist to ensure same key generation
    FILE* temp_file = fopen(pathname, "w");
    if (temp_file) {
        fprintf(temp_file, "Shared memory for chtml module %d", module_idx);
        fclose(temp_file);
    }
    
    return ftok(pathname, 'R' + module_idx); // 'R' is arbitrary project identifier
}

// Function to send response back through shared memory
void send_response(ModuleSharedMem* shmem_ptr, const char* response) {
    // Wait if previous response is still pending
    // In a real implementation, we might want to have a timeout
    int attempts = 0;
    while (shmem_ptr->response_ready && attempts < 100) {
        usleep(1000); // Sleep 1ms before retrying
        attempts++;
    }
    
    if (attempts >= 100) {
        printf("Warning: Could not send response, response buffer full\n");
        return;
    }
    
    // Copy response to shared buffer
    int response_len = strlen(response);
    if (response_len >= sizeof(shmem_ptr->response_buffer)) {
        printf("Error: Response too long for buffer\n");
        return;
    }
    
    strcpy(shmem_ptr->response_buffer, response);
    shmem_ptr->response_ready = 1;
    shmem_ptr->last_response_timestamp = time(NULL);
    
    // Signal the main process that a response is ready (semaphore 1)
    struct sembuf sb;
    sb.sem_num = 1;  // Response semaphore
    sb.sem_op = 1;   // Signal (increment)
    sb.sem_flg = 0;
    
    if (semop(shmem_ptr->sem_id, &sb, 1) == -1) {
        perror("semop signal response");
    }
}

// Function to update player position display
void update_player_display(ModuleSharedMem* shmem_ptr) {
    char response[4096];
    
    // Send position update
    snprintf(response, sizeof(response), 
             "VAR;position_display;string;Player Position: X: %.1f, Y: %.1f, Z: %.1f\n"
             "SHAPE;RECT;player;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f\n"
             "SHAPE;RECT;player_map;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f\n"
             "VAR;status_display;string;Player moved: X=%.1f, Y=%.1f\n",
             player_x, player_y, player_z,
             player_x * 20 + 300, player_y * 20 + 200, player_z, 20.0, 20.0, 20.0, 1.0, 0.0, 0.0, 1.0,
             player_x + 60, player_y + 60, 0.0, 5.0, 5.0, 5.0, 1.0, 1.0, 0.0, 1.0,
             player_x, player_y);
    
    send_response(shmem_ptr, response);
}

int main(int argc, char *argv[]) {
    int module_idx = 0; // Default to first module, can be specified as argument
    if (argc > 1) {
        module_idx = atoi(argv[1]);
    }
    
    // Generate the same key as the main application to connect to the shared memory segment
    key_t shm_key = generate_shm_key(module_idx);
    if (shm_key == -1) {
        perror("ftok for shared memory");
        return 1;
    }
    
    // Connect to the existing shared memory segment
    // Use IPC_EXCL to ensure we connect to an existing segment (not create a new one)
    int shmem_id = shmget(shm_key, sizeof(ModuleSharedMem), 0666);
    if (shmem_id == -1) {
        perror("shmget - could not connect to existing shared memory");
        return 1;
    }
    
    // Attach to shared memory
    ModuleSharedMem* shmem_ptr = (ModuleSharedMem*)shmat(shmem_id, NULL, 0);
    if (shmem_ptr == (ModuleSharedMem*)-1) {
        perror("shmat - could not attach to shared memory");
        return 1;
    }
    
    printf("Connected to shared memory segment with ID %d for module %d\n", shmem_id, module_idx);
    
    // Send initial status update
    char initial_response[2048];
    snprintf(initial_response, sizeof(initial_response),
             "VAR;page_status;string;Shared Memory Player Movement Module Loaded\n"
             "VAR;position_display;string;Player Position: X: %.1f, Y: %.1f, Z: %.1f\n"
             "SHAPE;RECT;player;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f\n"
             "SHAPE;RECT;player_map;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f\n"
             "VAR;status_display;string;Waiting for commands via shared memory...\n",
             player_x, player_y, player_z,
             player_x * 20 + 300, player_y * 20 + 200, player_z, 20.0, 20.0, 20.0, 1.0, 0.0, 0.0, 1.0,
             player_x + 60, player_y + 60, 0.0, 5.0, 5.0, 5.0, 1.0, 1.0, 0.0, 1.0);
    
    send_response(shmem_ptr, initial_response);
    
    // Main command processing loop - now using shared memory instead of stdin
    while (1) {
        // Wait for a command by checking the command semaphore
        struct sembuf sb;
        sb.sem_num = 0;  // Command semaphore
        sb.sem_op = -1;  // Wait (decrement)
        sb.sem_flg = 0;  // Block until available
        
        if (semop(shmem_ptr->sem_id, &sb, 1) == -1) {
            perror("semop wait for command");
            break; // Exit loop on error
        }
        
        // Process the command if it's ready
        if (shmem_ptr->command_ready) {
            // Process movement commands
            if (strcmp(shmem_ptr->command_buffer, "UP") == 0) {
                player_y += 1.0f;
            } else if (strcmp(shmem_ptr->command_buffer, "DOWN") == 0) {
                player_y -= 1.0f;
            } else if (strcmp(shmem_ptr->command_buffer, "LEFT") == 0) {
                player_x -= 1.0f;
            } else if (strcmp(shmem_ptr->command_buffer, "RIGHT") == 0) {
                player_x += 1.0f;
            } else if (strncmp(shmem_ptr->command_buffer, "EVENT:", 6) == 0) {
                // Handle dynamic events
                char* event_name = shmem_ptr->command_buffer + 6;
                if (strcmp(event_name, "player_reset") == 0) {
                    player_x = 0.0f;
                    player_y = 0.0f;
                    player_z = 0.0f;
                }
            }
            
            // Clear the command ready flag after processing
            shmem_ptr->command_ready = 0;
            
            // Update the display with the new player position
            update_player_display(shmem_ptr);
        }
    }
    
    // Detach from shared memory before exiting
    shmdt(shmem_ptr);
    
    return 0;
}