/*
 * user_manager.c - User Profile Manager
 * Manages user authentication state and interacts with CHTPM framework.
 * Reads/writes state from/to projects/user/manager/state.txt.
 * Integrates password hashing (SHA256) using OpenSSL.
 * Handles authentication screens (login, signup, profile) and input focus.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

// OpenSSL includes for hashing
#include <openssl/sha.h>
#include <openssl/evp.h>

#define STATE_FILE "projects/user/manager/state.txt"
#define LAYOUT_FILE "pieces/display/current_layout.txt" // For future use, not currently read
#define OPS_PATH "projects/user/ops/+x"

// --- State Management ---

typedef enum {
    AUTH_SCREEN_MAIN_MENU,
    AUTH_SCREEN_LOGIN,
    AUTH_SCREEN_SIGNUP,
    AUTH_SCREEN_PROFILE,
    AUTH_SCREEN_WAITING_FOR_OP
} AuthScreenState;

typedef struct {
    char username_input[256];
    char password_input[256];
    char auth_response[512];
    char session_status[50]; // e.g., IDLE, AUTHENTICATED, LOGGED_OUT
    char current_user[256];
    char last_key[10]; // To capture key presses from CHTPM (e.g., "1", "2", "27")
    AuthScreenState auth_screen;
    int active_target_id; // From CHTPM, used for input focus (e.g., 1 for username, 2 for password)
    char input_mode[50]; // From CHTPM, e.g., "text", "password"
} AppState;

// Helper to trim whitespace
void trim_whitespace(char *str) {
    char *end;
    // Trim leading space
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (*str == 0) return;
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    *(end + 1) = 0;
}

// Function to read state from state.txt
int read_state(AppState *state) {
    FILE *fp = fopen(STATE_FILE, "r");
    if (!fp) {
        // If file doesn't exist, initialize with defaults
        memset(state, 0, sizeof(AppState));
        state->auth_screen = AUTH_SCREEN_MAIN_MENU;
        strcpy(state->session_status, "IDLE");
        state->active_target_id = 1; // Default focus to username input
        strcpy(state->input_mode, "text");
        return 0; // Indicate initialization
    }

    char line[1024];
    char key[100], value[512];
    int success = 1; // Assume success initially

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%99[^=]=%511[^\n]", key, value) == 2) {
            trim_whitespace(value);
            if (strcmp(key, "username_input") == 0) strncpy(state->username_input, value, sizeof(state->username_input) - 1);
            else if (strcmp(key, "password_input") == 0) strncpy(state->password_input, value, sizeof(state->password_input) - 1);
            else if (strcmp(key, "auth_response") == 0) strncpy(state->auth_response, value, sizeof(state->auth_response) - 1);
            else if (strcmp(key, "session_status") == 0) strncpy(state->session_status, value, sizeof(state->session_status) - 1);
            else if (strcmp(key, "current_user") == 0) strncpy(state->current_user, value, sizeof(state->current_user) - 1);
            else if (strcmp(key, "last_key") == 0) strncpy(state->last_key, value, sizeof(state->last_key) - 1);
            else if (strcmp(key, "auth_screen") == 0) {
                int screen_val = atoi(value);
                if (screen_val >= AUTH_SCREEN_MAIN_MENU && screen_val <= AUTH_SCREEN_WAITING_FOR_OP) {
                    state->auth_screen = (AuthScreenState)screen_val;
                }
            } else if (strcmp(key, "active_target_id") == 0) state->active_target_id = atoi(value);
            else if (strcmp(key, "input_mode") == 0) strncpy(state->input_mode, value, sizeof(state->input_mode) - 1);
        }
    }
    fclose(fp);
    return success;
}

// Function to write state back to state.txt
int write_state(const AppState *state) {
    FILE *fp = fopen(STATE_FILE, "w");
    if (!fp) {
        perror("Failed to open state file for writing");
        return -1;
    }

    fprintf(fp, "username_input=%s\n", state->username_input);
    fprintf(fp, "password_input=%s\n", state->password_input);
    fprintf(fp, "auth_response=%s\n", state->auth_response);
    fprintf(fp, "session_status=%s\n", state->session_status);
    fprintf(fp, "current_user=%s\n", state->current_user);
    fprintf(fp, "last_key=%s\n", state->last_key);
    fprintf(fp, "auth_screen=%d\n", state->auth_screen);
    fprintf(fp, "active_target_id=%d\n", state->active_target_id);
    fprintf(fp, "input_mode=%s\n", state->input_mode);
    
    fclose(fp);
    return 0;
}

// --- Password Hashing ---

// Simple SHA256 hash function using modern EVP interface
int sha256_hash(const char *input, char *output_hex, size_t output_hex_len) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    
    if (mdctx == NULL) return -1;
    
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    if (EVP_DigestUpdate(mdctx, input, strlen(input)) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    EVP_MD_CTX_free(mdctx);

    for (unsigned int i = 0; i < hash_len && (i * 2 + 1) < output_hex_len; i++) {
        snprintf(output_hex + (i * 2), output_hex_len - (i * 2), "%02x", hash[i]);
    }
    
    return 0;
}

// --- Operation Calling ---

// Calls an operation from projects/user/ops/+x/op_name.+x
// The op is expected to read inputs from state.txt and write outputs to state.txt
int call_user_op(AppState *state, const char *op_name) {
    char cmd[1024];
    // Pass the state file path as an argument to the operation
    snprintf(cmd, sizeof(cmd), "%s/%s.+x %s", OPS_PATH, op_name, STATE_FILE);
    
    // Before calling, ensure state is written so the op can read it.
    // This is a critical synchronization point.
    if (write_state(state) != 0) {
        fprintf(stderr, "Error: Failed to write state before calling op '%s'\n", op_name);
        strncpy(state->auth_response, "Internal error: state write failed", sizeof(state->auth_response) - 1);
        state->auth_screen = AUTH_SCREEN_MAIN_MENU; // Reset to main menu on error
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        strncpy(state->auth_response, "System error: fork failed", sizeof(state->auth_response) - 1);
        state->auth_screen = AUTH_SCREEN_MAIN_MENU; // Reset to main menu on error
        return -1;
    }
    
    if (pid == 0) {
        /* Child process */
        setpgid(0, 0); // Create new process group
        // Execute the op. It's expected to read STATE_FILE and write back to it.
        execlp(cmd, cmd, NULL); // Use execlp to find cmd in PATH if needed
        perror("execlp failed"); // If execlp fails, print error and exit
        _exit(1);
    }
    
    /* Parent waits for child */
    int status;
    waitpid(pid, &status, 0);
    
    // After the op finishes, read the state again to get its results.
    if (read_state(state) != 0) {
        fprintf(stderr, "Error: Failed to read state after op '%s'\n", op_name);
        strncpy(state->auth_response, "Internal error: state read failed", sizeof(state->auth_response) - 1);
        state->auth_screen = AUTH_SCREEN_MAIN_MENU; // Reset to main menu on error
        return -1;
    }

    // Check for operation success/failure based on exit code and state.auth_response
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        // Op executed successfully. The state.auth_response should contain feedback.
        return 0;
    } else {
        // Op failed or returned an error. The state.auth_response should ideally contain the error message.
        fprintf(stderr, "Op '%s' failed with exit code %d\n", op_name, WEXITSTATUS(status));
        if (strlen(state->auth_response) == 0) {
             strncpy(state->auth_response, "Operation failed, unknown error", sizeof(state->auth_response) - 1);
        }
        state->auth_screen = AUTH_SCREEN_MAIN_MENU; // Reset to main menu on op failure
        return -1;
    }
}

// --- Signal Handling ---
static volatile sig_atomic_t g_shutdown = 0;

void handle_signal(int sig) {
    (void)sig;
    g_shutdown = 1;
}

int setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) < 0) { perror("sigaction SIGINT"); return -1; }
    if (sigaction(SIGTERM, &sa, NULL) < 0) { perror("sigaction SIGTERM"); return -1; }
    
    return 0;
}

// --- Main Loop ---
int main(void) {
    AppState state;
    
    /* Setup */
    setpgid(0, 0); // Make this process its own process group leader
    if (setup_signal_handlers() != 0) {
        fprintf(stderr, "Failed to setup signal handlers.\n");
        return 1;
    }
    
    printf("user_manager started. Reading state...\n");
    
    // Initialize state
    if (read_state(&state) != 0) {
        fprintf(stderr, "Warning: Initial state read failed, using defaults.\n");
    }
    
    // Ensure initial state is written out for CHTPM to pick up
    if (write_state(&state) != 0) {
        fprintf(stderr, "Error: Failed to write initial state.\n");
        return 1;
    }

    while (!g_shutdown) {
        // Read state to get latest inputs and CHTPM directives (like KEY presses, focus changes)
        if (read_state(&state) != 0) {
            fprintf(stderr, "Error reading state file, attempting to continue...\n");
            strncpy(state.auth_response, "System error: state read failed", sizeof(state.auth_response) - 1);
            state.auth_screen = AUTH_SCREEN_MAIN_MENU; // Reset screen on error
            if (write_state(&state) != 0) {
                 fprintf(stderr, "Critical error: could not write state after read error.\n");
                 g_shutdown = 1; // Exit if we can't even write state
            }
            usleep(500000); // Longer sleep on error
            continue;
        }

        // Process CHTPM directives (e.g., button clicks mapped to KEY:n)
        if (strlen(state.last_key) > 0) {
            printf("Received key: %s\n", state.last_key);
            
            // --- Handle Global Keys ---
            // ESC to exit
            if (strcmp(state.last_key, "27") == 0) { 
                g_shutdown = 1;
                // Don't clear last_key here, it signifies the exit command
            } 
            // Clear inputs
            else if (strcmp(state.last_key, "5") == 0) { 
                memset(state.username_input, 0, sizeof(state.username_input));
                memset(state.password_input, 0, sizeof(state.password_input));
                memset(state.auth_response, 0, sizeof(state.auth_response));
                // Stay on current screen or go to main menu? Let's go to main menu if not in a sub-screen.
                if (state.auth_screen != AUTH_SCREEN_LOGIN && state.auth_screen != AUTH_SCREEN_SIGNUP) {
                     state.auth_screen = AUTH_SCREEN_MAIN_MENU;
                }
                // Clear last_key after processing
                memset(state.last_key, 0, sizeof(state.last_key)); 
            } 
            // Back/Cancel
            else if (strcmp(state.last_key, "6") == 0) { 
                if (state.auth_screen == AUTH_SCREEN_LOGIN || state.auth_screen == AUTH_SCREEN_SIGNUP) {
                    state.auth_screen = AUTH_SCREEN_MAIN_MENU; // Go back to main menu
                    strncpy(state.auth_response, "Operation cancelled.", sizeof(state.auth_response) - 1);
                }
                memset(state.last_key, 0, sizeof(state.last_key));
            } 
            // --- Handle Screen-Specific Actions ---
            else { // If not a global key that was handled, process screen specific keys
                switch (state.auth_screen) {
                    case AUTH_SCREEN_MAIN_MENU:
                        if (strcmp(state.last_key, "1") == 0) { // [1] Create Profile button -> Go to Signup screen
                            state.auth_screen = AUTH_SCREEN_SIGNUP;
                            strncpy(state.auth_response, "Enter username and password for signup.", sizeof(state.auth_response) - 1);
                            memset(state.username_input, 0, sizeof(state.username_input)); // Clear inputs
                            memset(state.password_input, 0, sizeof(state.password_input));
                            memset(state.current_user, 0, sizeof(state.current_user));
                            state.active_target_id = 1; // Default focus to username input
                            strcpy(state.input_mode, "text");
                        } else if (strcmp(state.last_key, "2") == 0) { // [2] Login button -> Go to Login screen
                            state.auth_screen = AUTH_SCREEN_LOGIN;
                            strncpy(state.auth_response, "Enter username and password for login.", sizeof(state.auth_response) - 1);
                            memset(state.username_input, 0, sizeof(state.username_input)); // Clear inputs
                            memset(state.password_input, 0, sizeof(state.password_input));
                            memset(state.current_user, 0, sizeof(state.current_user));
                            state.active_target_id = 1; // Default focus to username input
                            strcpy(state.input_mode, "text");
                        } else if (strcmp(state.last_key, "4") == 0) { // [4] Logout button -> Perform logout operation
                            strncpy(state.auth_response, "Logging out...", sizeof(state.auth_response) - 1);
                            state.auth_screen = AUTH_SCREEN_WAITING_FOR_OP;
                            // Simulate successful logout by resetting state
                            memset(state.current_user, 0, sizeof(state.current_user));
                            strncpy(state.session_status, "LOGGED_OUT", sizeof(state.session_status) - 1);
                            state.auth_screen = AUTH_SCREEN_MAIN_MENU; // Transition back to main menu
                            strncpy(state.auth_response, "Logged out successfully.", sizeof(state.auth_response) - 1);
                            // In a real scenario, call_user_op(&state, "logout"); would be here.
                        }
                        break;

                    case AUTH_SCREEN_LOGIN:
                        if (strcmp(state.last_key, "3") == 0) { // [3] Login button -> Perform login operation
                            char hashedPassword[SHA256_DIGEST_LENGTH * 2 + 1];
                            if (sha256_hash(state.password_input, hashedPassword, sizeof(hashedPassword)) == 0) {
                                strncpy(state.auth_response, "Authenticating...", sizeof(state.auth_response) - 1);
                                state.auth_screen = AUTH_SCREEN_WAITING_FOR_OP;
                                state.active_target_id = 0; // No active input target while waiting
                                
                                // Call the auth_user operation. It is expected to read state.txt,
                                // perform authentication (potentially hashing the input password for lookup if needed),
                                // and update state.txt with results.
                                if (call_user_op(&state, "auth_user") != 0) {
                                    // Error during op call, state updated by call_user_op
                                    // call_user_op already sets auth_screen to MAIN_MENU on critical error
                                } else {
                                    // Op finished, check state.auth_response and state.session_status
                                    if (strcmp(state.session_status, "AUTHENTICATED") == 0) {
                                        strncpy(state.auth_response, "Login successful!", sizeof(state.auth_response) - 1);
                                        state.auth_screen = AUTH_SCREEN_PROFILE; // Move to profile view
                                    } else {
                                        strncpy(state.auth_response, "Login failed. Check credentials.", sizeof(state.auth_response) - 1);
                                        state.auth_screen = AUTH_SCREEN_LOGIN; // Stay on login screen
                                    }
                                }
                            } else {
                                strncpy(state.auth_response, "Error hashing password for login.", sizeof(state.auth_response) - 1);
                                state.auth_screen = AUTH_SCREEN_LOGIN;
                            }
                        }
                        // Note: KEY:1 and KEY:2 on login screen are handled by CHTPM to set active_target_id and update
                        // username_input/password_input state.last_key is not used for input changes.
                        break;

                    case AUTH_SCREEN_SIGNUP:
                        if (strcmp(state.last_key, "4") == 0) { // [4] Signup button -> Perform signup operation
                            char hashedPassword[SHA256_DIGEST_LENGTH * 2 + 1];
                            if (sha256_hash(state.password_input, hashedPassword, sizeof(hashedPassword)) == 0) {
                                strncpy(state.auth_response, "Creating profile...", sizeof(state.auth_response) - 1);
                                state.auth_screen = AUTH_SCREEN_WAITING_FOR_OP;
                                state.active_target_id = 0; // No active input target while waiting
                                
                                // Call the create_profile operation. It's expected to read inputs from state.txt,
                                // hash the password, store it, and update state.txt with results.
                                if (call_user_op(&state, "create_profile") != 0) {
                                    // Error during op call, state updated by call_user_op
                                } else {
                                    // Check state.auth_response after op completes
                                    if (strcmp(state.auth_response, "Profile created successfully") == 0) { // Assuming op sets this response
                                        state.auth_screen = AUTH_SCREEN_LOGIN; // Go to login screen after successful signup
                                    } else {
                                        state.auth_screen = AUTH_SCREEN_SIGNUP; // Stay on signup screen if failed
                                    }
                                }
                            } else {
                                strncpy(state.auth_response, "Error hashing password for signup.", sizeof(state.auth_response) - 1);
                                state.auth_screen = AUTH_SCREEN_SIGNUP;
                            }
                        }
                        // Note: KEY:1 and KEY:2 are for input focus handled by CHTPM.
                        break;

                    case AUTH_SCREEN_PROFILE:
                        // Only logout (KEY:4) is explicitly handled here. Other keys might be ignored or handled globally.
                        // ESC (KEY:27) is handled globally.
                        break;
                    
                    case AUTH_SCREEN_WAITING_FOR_OP:
                        // No specific key actions here, wait for op to complete and update screen.
                        // ESC (KEY:27) is handled globally.
                        break;
                }
                
                // Clear last_key after processing all logic for this turn,
                // unless it was ESC which caused shutdown.
                if (g_shutdown == 0) { // If we are not shutting down due to ESC
                   // Ensure last_key is cleared if it was processed and not ESC
                   memset(state.last_key, 0, sizeof(state.last_key));
                }
            }
        } else {
            // No key pressed, ensure active_target_id is set correctly based on screen
            if (state.auth_screen == AUTH_SCREEN_LOGIN || state.auth_screen == AUTH_SCREEN_SIGNUP) {
                // If no key was pressed but we are in login/signup, ensure focus is on the current target_id
                // This is mainly for when CHTPM might lose focus or upon initial screen load.
                // The CHTPM itself should handle setting active_target_id based on user interaction.
                // Here, we ensure it's set if it's zero or invalid, defaulting to username.
                if (state.active_target_id == 0) {
                    state.active_target_id = 1;
                    strcpy(state.input_mode, "text"); // Default input mode
                }
            }
        }

        // Write updated state back to file so CHTPM can read it for UI updates.
        if (write_state(&state) != 0) {
            fprintf(stderr, "Error writing state file in main loop.\n");
            // Consider error handling or shutdown if state persistence is critical
        }
        
        // Small sleep to prevent busy-waiting
        usleep(50000); // 50ms poll rate
    }
    
    printf("user_manager shutting down.\n");
    
    // Final cleanup/write before exit
    if (write_state(&state) != 0) {
        fprintf(stderr, "Error writing final state on shutdown.\n");
    }
    
    return 0;
}
