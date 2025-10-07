#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_PATH_LEN 1024
#define MAX_PLAYLIST_SIZE 1000
#define COMMAND_FILE "player_command.txt"
#define STATUS_FILE "player_status.txt"
#define PLAYLIST_FILE "playlist.txt"
#define POLL_INTERVAL_MS 100
#define VISIBLE_SONGS 40
#define TRACK_X 550
#define TRACK_WIDTH 20
#define TRACK_TOP 900
#define TRACK_BOTTOM 100
#define TRACK_HEIGHT (TRACK_TOP - TRACK_BOTTOM)

// Global variables
char playlist[MAX_PLAYLIST_SIZE][MAX_PATH_LEN];
int playlist_size = 0;
int current_song_index = 0;
char player_state[50] = "STOPPED";
char current_song_path[MAX_PATH_LEN] = "N/A";
char playback_time[50] = "0.00/0.00";
int scroll_offset = 0;
int dragging_thumb = 0;
int manual_scrolling = 0;
float initial_mouse_y = 0;
float initial_offset = 0;
int in_search = 0;
char search_string[MAX_PATH_LEN] = "";
int highlighted_index = -1;
int mouse_x = 0, mouse_y = 0; // Track mouse position for hover detection
int shuffle_state = 0; // 0 = OFF, 1 = ON

// Button definitions
typedef struct {
    int x, y, width, height;
    char *label;
} Button;

Button play_button = {50, 980, 60, 30, "Play"};
Button pause_button = {120, 980, 60, 30, "Pause"};
Button stop_button = {190, 980, 60, 30, "Stop"};
Button next_button = {260, 980, 60, 30, "Next"};
Button prev_button = {330, 980, 60, 30, "Prev"};
Button search_button = {400, 980, 60, 30, "Search"};
Button shuffle_button = {470, 980, 60, 30, "Shuffle OFF"};

// --- Utility Functions ---
void write_command(const char *command) {
    FILE *f = fopen(COMMAND_FILE, "w");
    if (f) {
        fprintf(f, "%s\n", command);
        fclose(f);
        printf("Sent command: %s\n", command);
    } else {
        perror("Error writing command file");
    }
}

void read_status(void) {
    FILE *f = fopen(STATUS_FILE, "r");
    if (f) {
        char line[MAX_PATH_LEN];
        while (fgets(line, MAX_PATH_LEN, f)) {
            if (strncmp(line, "STATE:", 6) == 0) {
                sscanf(line, "STATE: %s", player_state);
            } else if (strncmp(line, "SHUFFLE:", 8) == 0) {
                char shuffle_str[10];
                sscanf(line, "SHUFFLE: %s", shuffle_str);
                shuffle_state = (strcmp(shuffle_str, "ON") == 0) ? 1 : 0;
                // Update the shuffle button label
                shuffle_button.label = shuffle_state ? "Shuffle ON" : "Shuffle OFF";
            } else if (strncmp(line, "CURRENT_SONG_INDEX:", 19) == 0) {
                sscanf(line, "CURRENT_SONG_INDEX: %d", &current_song_index);
            } else if (strncmp(line, "CURRENT_SONG_PATH:", 18) == 0) {
                sscanf(line, "CURRENT_SONG_PATH: %s", current_song_path);
            } else if (strncmp(line, "PLAYBACK_TIME:", 14) == 0) {
                sscanf(line, "PLAYBACK_TIME: %s", playback_time);
            }
        }
        fclose(f);
    }
}

void read_playlist(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Error opening playlist.txt");
        return;
    }
    playlist_size = 0;
    char buffer[MAX_PATH_LEN];
    int line_count = 0;
    while (fgets(buffer, MAX_PATH_LEN, f) != NULL && playlist_size < MAX_PLAYLIST_SIZE) {
        line_count++;
        size_t len = strcspn(buffer, "\n");
        buffer[len] = '\0';
        if (strlen(buffer) == 0) {
            printf("Skipping empty line %d in playlist.txt\n", line_count);
            continue;
        }
        char *filename = strrchr(buffer, '/');
        if (!filename) filename = strrchr(buffer, '\\');
        if (!filename) filename = buffer;
        else filename++;
        if (strlen(filename) == 0 || strlen(filename) >= MAX_PATH_LEN - 1) {
            printf("Skipping invalid filename at line %d: %s\n", line_count, filename);
            continue;
        }
        strncpy(playlist[playlist_size], filename, MAX_PATH_LEN - 1);
        playlist[playlist_size][MAX_PATH_LEN - 1] = '\0';
        printf("Loaded song %d: %s\n", playlist_size + 1, playlist[playlist_size]);
        playlist_size++;
    }
    fclose(f);
    printf("Loaded %d songs from playlist.txt (%d lines processed)\n", playlist_size, line_count);
    if (playlist_size == 0) {
        fprintf(stderr, "Warning: No valid songs loaded from playlist.txt\n");
    }
    if (playlist_size > 176) {
        printf("Song at index 176: %s\n", playlist[176]);
    } else {
        printf("Playlist size %d, no song at index 176\n", playlist_size);
    }
}

void perform_search(void) {
    printf("Performing search for: '%s'\n", search_string);
    highlighted_index = -1;
    if (strlen(search_string) == 0) {
        printf("Search string empty, clearing highlight\n");
        return;
    }

    for (int i = 0; i < playlist_size; i++) {
        if (strstr(playlist[i], search_string) != NULL) {
            printf("Match found at index %d: '%s'\n", i, playlist[i]);
            highlighted_index = i;
            break; // Highlight first match
        }
    }
    if (highlighted_index == -1) {
        printf("No matches found for '%s'\n", search_string);
    } else {
        printf("Highlighted index %d: '%s'\n", highlighted_index, playlist[highlighted_index]);
        // Auto-scroll to make the highlighted song visible
        if (highlighted_index < scroll_offset || highlighted_index >= scroll_offset + VISIBLE_SONGS) {
            scroll_offset = highlighted_index - VISIBLE_SONGS / 2;
            if (scroll_offset < 0) scroll_offset = 0;
            int max_off = playlist_size - VISIBLE_SONGS;
            if (max_off < 0) max_off = 0;
            if (scroll_offset > max_off) scroll_offset = max_off;
            printf("Set scroll_offset to %d to center index %d\n", scroll_offset, highlighted_index);
        }
        manual_scrolling = 0;
    }
}

// --- OpenGL/GLUT Functions ---
void draw_text(float x, float y, char *string) {
    glRasterPos2f(x, y);
    for (char *c = string; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
}

void draw_button(Button btn) {
    glColor3f(0.2, 0.2, 0.2);
    glBegin(GL_QUADS);
    glVertex2f(btn.x, btn.y);
    glVertex2f(btn.x + btn.width, btn.y);
    glVertex2f(btn.x + btn.width, btn.y + btn.height);
    glVertex2f(btn.x, btn.y + btn.height);
    glEnd();
    glColor3f(1.0, 1.0, 1.0);
    draw_text(btn.x + 10, btn.y + 15, btn.label);
}

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1.0, 1.0, 1.0);

    // Draw player status at the top
    glColor3f(0.0, 1.0, 0.0); // Green color for status
    char *status_str = malloc(MAX_PATH_LEN + 200);
    if (status_str == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        glutSwapBuffers();
        return;
    }
    
    snprintf(status_str, MAX_PATH_LEN + 200, "Player Status: %s", player_state);
    draw_text(50, 1080, status_str);
    if (current_song_index >= 0 && current_song_index < playlist_size && playlist_size > 0) {
        snprintf(status_str, MAX_PATH_LEN + 200, "Now Playing: %d - %s", current_song_index, playlist[current_song_index]);
    } else {
        snprintf(status_str, MAX_PATH_LEN + 200, "Now Playing: -");
    }
    draw_text(50, 1060, status_str);
    snprintf(status_str, MAX_PATH_LEN + 200, "Time: %s", playback_time);
    draw_text(50, 1040, status_str);
    glColor3f(1.0, 1.0, 1.0); // Reset to white for other text

    // Draw control buttons
    draw_button(play_button);
    draw_button(pause_button);
    draw_button(stop_button);
    draw_button(next_button);
    draw_button(prev_button);
    draw_button(search_button);
    draw_button(shuffle_button);

    // Draw search input with purple color
    glColor3f(0.5, 0.0, 0.5); // Purple color for search
    snprintf(status_str, MAX_PATH_LEN + 200, "Search: %s_", search_string);
    draw_text(50, TRACK_TOP + 40, status_str);
    glColor3f(1.0, 1.0, 1.0); // Reset to white for other text

    // Draw playlist with hover highlighting
    draw_text(50, TRACK_TOP + 20, "Playlist:");
    int max_visible = scroll_offset + VISIBLE_SONGS < playlist_size ? scroll_offset + VISIBLE_SONGS : playlist_size;
    for (int i = scroll_offset; i < max_visible; i++) {
        snprintf(status_str, MAX_PATH_LEN + 200, "%d. %s", i, playlist[i]);
        int song_y = TRACK_TOP - ((i - scroll_offset) * 18);
        
        // Check if mouse is hovering over this song (more precise detection)
        if (mouse_x >= 50 && mouse_x <= 540 && mouse_y >= song_y - 8 && mouse_y <= song_y + 8) {
            // Draw highlight rectangle for hover
            glColor3f(0.3, 0.3, 0.3);
            glBegin(GL_QUADS);
            glVertex2f(45, song_y - 8);
            glVertex2f(545, song_y - 8);
            glVertex2f(545, song_y + 8);
            glVertex2f(45, song_y + 8);
            glEnd();
        }
        
        // Draw the song text
        if (i == current_song_index) {
            glColor3f(1.0, 1.0, 0.0); // Yellow for current song
        } else if (i == highlighted_index) {
            glColor3f(0.0, 1.0, 1.0); // Cyan for search highlight
        } else {
            glColor3f(1.0, 1.0, 1.0); // White for normal songs
        }
        draw_text(50, song_y, status_str);
    }

    // Draw scroll bar
    glColor3f(0.5, 0.5, 0.5);
    glBegin(GL_QUADS);
    glVertex2f(TRACK_X, TRACK_BOTTOM);
    glVertex2f(TRACK_X + TRACK_WIDTH, TRACK_BOTTOM);
    glVertex2f(TRACK_X + TRACK_WIDTH, TRACK_TOP);
    glVertex2f(TRACK_X, TRACK_TOP);
    glEnd();

    if (playlist_size > VISIBLE_SONGS) {
        float max_off = playlist_size - VISIBLE_SONGS;
        float thumb_h = (float)VISIBLE_SONGS / playlist_size * TRACK_HEIGHT;
        float prop = (float)scroll_offset / max_off;
        float effective = TRACK_HEIGHT - thumb_h;
        float thumb_top = TRACK_TOP - prop * effective;
        float thumb_bottom = thumb_top - thumb_h;
        glColor3f(0.8, 0.8, 0.8);
        glBegin(GL_QUADS);
        glVertex2f(TRACK_X, thumb_bottom);
        glVertex2f(TRACK_X + TRACK_WIDTH, thumb_bottom);
        glVertex2f(TRACK_X + TRACK_WIDTH, thumb_top);
        glVertex2f(TRACK_X, thumb_top);
        glEnd();
    }
    
    free(status_str);
    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    if (in_search) {
        if (key == 13) { // Enter
            printf("Search triggered with: '%s'\n", search_string);
            perform_search();
            in_search = 0;
            glutPostRedisplay();
        } else if (key == 8 || key == 127) { // Backspace
            size_t len = strlen(search_string);
            if (len > 0) {
                search_string[len - 1] = '\0';
                printf("Search string updated: '%s'\n", search_string);
            }
            glutPostRedisplay();
        } else if (key == 27) { // ESC
            in_search = 0;
            search_string[0] = '\0';
            highlighted_index = -1;
            printf("Search cleared\n");
            glutPostRedisplay();
        } else if (isprint(key)) {
            size_t len = strlen(search_string);
            if (len < MAX_PATH_LEN - 1) {
                search_string[len] = key;
                search_string[len + 1] = '\0';
                printf("Search string updated: '%s'\n", search_string);
            }
            glutPostRedisplay();
        }
    } else {
        if (key == '/') {
            in_search = 1;
            search_string[0] = '\0';
            highlighted_index = -1;
            printf("Entered search mode\n");
            glutPostRedisplay();
        } else if (key == 27) { // ESC
            exit(0);
        }
    }
}

void special_keyboard(int key, int x, int y) {
    if (in_search) return;

    int max_off = playlist_size - VISIBLE_SONGS;
    if (max_off < 0) max_off = 0;

    switch (key) {
        case GLUT_KEY_UP:
            if (current_song_index > 0) {
                int new_index = current_song_index - 1;
                char cmd[50];
                sprintf(cmd, "PLAY %d", new_index);
                write_command(cmd);
                manual_scrolling = 0;
            }
            break;
        case GLUT_KEY_DOWN:
            if (current_song_index < playlist_size - 1) {
                int new_index = current_song_index + 1;
                char cmd[50];
                sprintf(cmd, "PLAY %d", new_index);
                write_command(cmd);
                manual_scrolling = 0;
            }
            break;
        case GLUT_KEY_PAGE_UP:
            scroll_offset -= VISIBLE_SONGS / 2;
            if (scroll_offset < 0) scroll_offset = 0;
            manual_scrolling = 1;
            break;
        case GLUT_KEY_PAGE_DOWN:
            scroll_offset += VISIBLE_SONGS / 2;
            if (scroll_offset > max_off) scroll_offset = max_off;
            manual_scrolling = 1;
            break;
    }
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    printf("Mouse click: button=%d, state=%d, x=%d, y=%d\n", button, state, x, y);
    y = glutGet(GLUT_WINDOW_HEIGHT) - y;
    printf("Adjusted y: %d\n", y);

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // Check if mouse is over the scrollbar area first
        int max_off = playlist_size - VISIBLE_SONGS;
        if (max_off < 0) max_off = 0;
        
        if (x >= TRACK_X && x <= TRACK_X + TRACK_WIDTH &&
            y >= TRACK_BOTTOM && y <= TRACK_TOP) {
            if (playlist_size > VISIBLE_SONGS) {
                float thumb_h = (float)VISIBLE_SONGS / playlist_size * TRACK_HEIGHT;
                float prop = (float)scroll_offset / max_off;
                float effective = TRACK_HEIGHT - thumb_h;
                float thumb_top = TRACK_TOP - prop * effective;
                float thumb_bottom = thumb_top - thumb_h;

                if (y >= thumb_bottom && y <= thumb_top) {
                    dragging_thumb = 1;
                    initial_mouse_y = y;
                    initial_offset = scroll_offset;
                    manual_scrolling = 1;
                } else {
                    float dist_from_top = TRACK_TOP - y;
                    float p = dist_from_top / TRACK_HEIGHT;
                    scroll_offset = (int)(p * max_off + 0.5);
                    if (scroll_offset > max_off) scroll_offset = max_off;
                    if (scroll_offset < 0) scroll_offset = 0;
                    manual_scrolling = 1;
                }
            }
        } else {
            // Check for button clicks
            if (x >= play_button.x && x <= play_button.x + play_button.width &&
                y >= play_button.y && y <= play_button.y + play_button.height) {
                printf("Play button clicked\n");
                char cmd[50];
                sprintf(cmd, "PLAY %d", current_song_index);
                write_command(cmd);
                manual_scrolling = 0;
            } else if (x >= pause_button.x && x <= pause_button.x + pause_button.width &&
                       y >= pause_button.y && y <= pause_button.y + pause_button.height) {
                printf("Pause button clicked\n");
                write_command("PAUSE");
            } else if (x >= stop_button.x && x <= stop_button.x + stop_button.width &&
                       y >= stop_button.y && y <= stop_button.y + stop_button.height) {
                printf("Stop button clicked\n");
                write_command("STOP");
            } else if (x >= next_button.x && x <= next_button.x + next_button.width &&
                       y >= next_button.y && y <= next_button.y + next_button.height) {
                printf("Next button clicked\n");
                write_command("NEXT");
                manual_scrolling = 0;
            } else if (x >= prev_button.x && x <= prev_button.x + prev_button.width &&
                       y >= prev_button.y && y <= prev_button.y + prev_button.height) {
                printf("Prev button clicked\n");
                write_command("PREV");
                manual_scrolling = 0;
            } else if (x >= search_button.x && x <= search_button.x + search_button.width &&
                       y >= search_button.y && y <= search_button.y + search_button.height) {
                printf("Search button clicked\n");
                in_search = 1;
                search_string[0] = '\0';
                highlighted_index = -1;
                printf("Entered search mode via button click\n");
                glutPostRedisplay();
            } else if (x >= shuffle_button.x && x <= shuffle_button.x + shuffle_button.width &&
                       y >= shuffle_button.y && y <= shuffle_button.y + shuffle_button.height) {
                printf("Shuffle button clicked\n");
                // Toggle shuffle state
                if (shuffle_state) {
                    write_command("SHUFFLE_OFF");
                    shuffle_state = 0;
                    shuffle_button.label = "Shuffle OFF";
                    printf("Shuffle OFF\n");
                } else {
                    write_command("SHUFFLE_ON");
                    shuffle_state = 1;
                    shuffle_button.label = "Shuffle ON";
                    printf("Shuffle ON\n");
                }
                glutPostRedisplay();
            } else {
                printf("No button clicked, checking for song selection\n");
                // Check for song selection (only if not over scrollbar)
                for (int i = scroll_offset; i < scroll_offset + VISIBLE_SONGS && i < playlist_size; i++) {
                    int song_y = TRACK_TOP - ((i - scroll_offset) * 18);
                    if (x >= 50 && x <= 540 && y >= song_y - 8 && y <= song_y + 8) {
                        printf("Song %d clicked\n", i);
                        char cmd[50];
                        sprintf(cmd, "PLAY %d", i);
                        write_command(cmd);
                        manual_scrolling = 0;
                        break;
                    }
                }
            }
        }
    } else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        dragging_thumb = 0;
    }

    glutPostRedisplay();
}

void motion(int x, int y) {
    y = glutGet(GLUT_WINDOW_HEIGHT) - y;
    mouse_x = x;
    mouse_y = y;

    if (dragging_thumb && playlist_size > VISIBLE_SONGS) {
        float max_off = playlist_size - VISIBLE_SONGS;
        float thumb_h = (float)VISIBLE_SONGS / playlist_size * TRACK_HEIGHT;
        float effective = TRACK_HEIGHT - thumb_h;
        if (effective <= 0) effective = 1;
        float delta_y = y - initial_mouse_y;
        float new_off = initial_offset - delta_y * (max_off / effective);
        if (new_off < 0) new_off = 0;
        if (new_off > max_off) new_off = max_off;
        scroll_offset = (int)(new_off + 0.5);
        manual_scrolling = 1;
        glutPostRedisplay();
    }
}

void reshape(int w, int h) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0, 0, w, h);
}

void timer(int value) {
    read_status();
    if (!manual_scrolling) {
        if (current_song_index >= 0 && current_song_index < playlist_size) {
            if (current_song_index < scroll_offset) {
                scroll_offset = current_song_index;
            } else if (current_song_index >= scroll_offset + VISIBLE_SONGS) {
                scroll_offset = current_song_index - VISIBLE_SONGS + 1;
            }
        }
        else {
            // When current_song_index is out of bounds, keep the current scroll position or reset to a safe value
            if (playlist_size > 0 && scroll_offset > playlist_size - VISIBLE_SONGS && playlist_size > VISIBLE_SONGS) {
                scroll_offset = playlist_size - VISIBLE_SONGS;
            } else if (playlist_size <= VISIBLE_SONGS && scroll_offset > 0) {
                scroll_offset = 0;
            }
        }
    }
    glutPostRedisplay();
    glutTimerFunc(POLL_INTERVAL_MS, timer, 0);
}

int main(int argc, char **argv) {
    read_playlist(PLAYLIST_FILE);
    if (playlist_size == 0) {
        fprintf(stderr, "No songs found in playlist.txt. Please run find_mp3s first.\n");
        return 1;
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(600, 1100);
    glutCreateWindow("MP3 Player GUI");

    glClearColor(0.1, 0.1, 0.1, 1.0);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(motion); // Track mouse movement even when no buttons are pressed
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keyboard);
    glutTimerFunc(0, timer, 0);

    printf("MP3 Player GUI started. Ensure mp3_player_cli is running.\n");
    glutMainLoop();
    return 0;
}
