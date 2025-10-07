
#include <GL/glut.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

#define M_PI 3.14159265358979323846
#define NUM_ENEMIES 3
#define MAX_PROJECTILES 10
#define MAX_ENEMY_PROJECTILES 20
#define SENSITIVITY 0.1f
#define MOVE_SPEED 0.1f
#define PROJECTILE_SPEED 0.5f
#define ENEMY_PROJECTILE_SPEED 2.5f
#define PROJECTILE_LIFETIME 2.0f
#define ENEMY_SHOOT_INTERVAL 1.0f
#define PLAYER_MAX_HEALTH 100.0f
#define ENEMY_MAX_HEALTH 3.0f
#define ENEMY_MOVE_SPEED 0.02f
#define PROJECTILE_RADIUS 0.05f
#define PLAYER_RADIUS 0.5f
#define ASCII_WIDTH 40
#define ASCII_HEIGHT 20
#define WORLD_MIN -20.0f
#define WORLD_MAX 20.0f

// Camera variables
float x = 0.0f, y = 1.0f, z = 5.0f;
float yaw = -90.0f;
float pitch = 0.0f;
float front_x, front_y, front_z;
float player_health = PLAYER_MAX_HEALTH;

// Mouse control
int mouse_last_y;
int mouse_first_entry = 1;
int window_width = 800, window_height = 600;

// ASCII rendering toggle
int ascii_enabled = 0;

// Structures
typedef struct {
    float x, y, z;
    float radius;
    int alive;
    float health;
    float last_shot_time;
} Enemy;

typedef struct {
    float x, y, z;
    float dx, dy, dz;
    float time_alive;
    int active;
} Projectile;

typedef struct {
    float x, y, z;
    float yaw, pitch;
    float health;
    Enemy enemies[NUM_ENEMIES];
    Projectile projectiles[MAX_PROJECTILES];
    Projectile enemy_projectiles[MAX_ENEMY_PROJECTILES];
} GameState;

Enemy enemies[NUM_ENEMIES];
Projectile projectiles[MAX_PROJECTILES];
Projectile enemy_projectiles[MAX_ENEMY_PROJECTILES];

// Timing
double last_time = 0.0;

// Function prototypes
void update_camera_direction();
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int mx, int my);
void special_keys(int key, int mx, int my);
void passive_mouse_motion(int mx, int my);
void shoot();
void enemy_shoot(int enemy_idx);
void update_projectiles();
void update_enemies();
void check_collisions();
void draw_hud(GLdouble modelview[16], GLdouble projection[16], GLint viewport[4]);
void draw_gun();
void draw_ascii();
void save_game(const char* filename, GameState* state);
void load_game(const char* filename, GameState* state);
void idle();
void init();

void update_camera_direction() {
    front_x = cos(yaw * M_PI / 180.0f) * cos(pitch * M_PI / 180.0f);
    front_y = sin(pitch * M_PI / 180.0f);
    front_z = sin(yaw * M_PI / 180.0f) * cos(pitch * M_PI / 180.0f);
}

void draw_hud(GLdouble modelview[16], GLdouble projection[16], GLint viewport[4]) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);

    // Player position and health
    char hud_text[64];
    snprintf(hud_text, sizeof(hud_text), "Pos: (%.1f, %.1f, %.1f)  Health: %.0f", x, y, z, player_health);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2i(10, 20);
    for (char *c = hud_text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }

    // Enemy health bars
    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (!enemies[i].alive) continue;

        GLdouble winX, winY, winZ;
        int success = gluProject(enemies[i].x, enemies[i].y + 1.0f, enemies[i].z,
                                modelview, projection, viewport,
                                &winX, &winY, &winZ);

        if (success && winZ >= 0 && winZ <= 1) {
            float health_ratio = enemies[i].health / ENEMY_MAX_HEALTH;
            glBegin(GL_QUADS);
            glColor3f(1.0f - health_ratio, health_ratio, 0.0f);
            glVertex2f(winX - 20, window_height - (winY - 5));
            glVertex2f(winX - 20 + 40 * health_ratio, window_height - (winY - 5));
            glVertex2f(winX - 20 + 40 * health_ratio, window_height - winY);
            glVertex2f(winX - 20, window_height - winY);
            glEnd();

            glColor3f(0.0f, 0.0f, 0.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(winX - 20, window_height - (winY - 5));
            glVertex2f(winX - 20 + 40 * health_ratio, window_height - (winY - 5));
            glVertex2f(winX - 20 + 40 * health_ratio, window_height - winY);
            glVertex2f(winX - 20, window_height - winY);
            glEnd();
        }
    }

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void draw_gun() {
    glColor3f(0.0f, 0.5f, 1.0f);
    glPushMatrix();
    glTranslatef(0.3f, -0.3f, -0.8f);
    glScalef(0.1f, 0.1f, 0.6f);
    glutSolidCube(1.0f);
    glPopMatrix();
}

void draw_ascii() {
    char ascii_map[ASCII_HEIGHT][ASCII_WIDTH + 1];
    int i, j;

    for (i = 0; i < ASCII_HEIGHT; i++) {
        for (j = 0; j < ASCII_WIDTH; j++) {
            ascii_map[i][j] = '.';
        }
        ascii_map[i][ASCII_WIDTH] = '\0';
    }

    int player_grid_x = (int)((x - WORLD_MIN) * ASCII_WIDTH / (WORLD_MAX - WORLD_MIN));
    int player_grid_z = (int)((z - WORLD_MIN) * ASCII_HEIGHT / (WORLD_MAX - WORLD_MIN));
    if (player_grid_x >= 0 && player_grid_x < ASCII_WIDTH && player_grid_z >= 0 && player_grid_z < ASCII_HEIGHT) {
        ascii_map[player_grid_z][player_grid_x] = '@';
    }

    for (i = 0; i < NUM_ENEMIES; i++) {
        if (!enemies[i].alive) continue;
        int enemy_grid_x = (int)((enemies[i].x - WORLD_MIN) * ASCII_WIDTH / (WORLD_MAX - WORLD_MIN));
        int enemy_grid_z = (int)((enemies[i].z - WORLD_MIN) * ASCII_HEIGHT / (WORLD_MAX - WORLD_MIN));
        if (enemy_grid_x >= 0 && player_grid_x < ASCII_WIDTH && enemy_grid_z >= 0 && enemy_grid_z < ASCII_HEIGHT) {
            ascii_map[enemy_grid_z][enemy_grid_x] = 'E';
        }
    }

    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;
        int proj_grid_x = (int)((projectiles[i].x - WORLD_MIN) * ASCII_WIDTH / (WORLD_MAX - WORLD_MIN));
        int proj_grid_z = (int)((projectiles[i].z - WORLD_MIN) * ASCII_HEIGHT / (WORLD_MAX - WORLD_MIN));
        if (proj_grid_x >= 0 && proj_grid_x < ASCII_WIDTH && proj_grid_z >= 0 && proj_grid_z < ASCII_HEIGHT) {
            ascii_map[proj_grid_z][proj_grid_x] = '*';
        }
    }

    for (i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        if (!enemy_projectiles[i].active) continue;
        int eproj_grid_x = (int)((enemy_projectiles[i].x - WORLD_MIN) * ASCII_WIDTH / (WORLD_MAX - WORLD_MIN));
        int eproj_grid_z = (int)((enemy_projectiles[i].z - WORLD_MIN) * ASCII_HEIGHT / (WORLD_MAX - WORLD_MIN));
        if (eproj_grid_x >= 0 && eproj_grid_x < ASCII_WIDTH && eproj_grid_z >= 0 && eproj_grid_z < ASCII_HEIGHT) {
            ascii_map[eproj_grid_z][eproj_grid_x] = 'o';
        }
    }

#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif

    printf("Player Health: %.0f\n", player_health);
    printf("Position: (%.1f, %.1f, %.1f)\n", x, y, z);
    printf("Enemies Alive: %d\n", NUM_ENEMIES);
    printf("\n");

    for (i = 0; i < ASCII_HEIGHT; i++) {
        printf("%s\n", ascii_map[i]);
    }
}

void save_game(const char* filename, GameState* state) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error: Could not open file %s for saving.\n", filename);
        return;
    }

    fprintf(file, "player: x=%.3f y=%.3f z=%.3f health=%.1f yaw=%.3f pitch=%.3f\n",
            state->x, state->y, state->z, state->health, state->yaw, state->pitch);
    fprintf(file, "enemies: count=%d\n", NUM_ENEMIES);
    for (int i = 0; i < NUM_ENEMIES; i++) {
        fprintf(file, "enemy%d: x=%.3f y=%.3f z=%.3f health=%.1f alive=%d\n",
                i, state->enemies[i].x, state->enemies[i].y, state->enemies[i].z,
                state->enemies[i].health, state->enemies[i].alive);
    }
    fprintf(file, "projectiles: count=%d\n", MAX_PROJECTILES);
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        fprintf(file, "projectile%d: x=%.3f y=%.3f z=%.3f dx=%.3f dy=%.3f dz=%.3f active=%d\n",
                i, state->projectiles[i].x, state->projectiles[i].y, state->projectiles[i].z,
                state->projectiles[i].dx, state->projectiles[i].dy, state->projectiles[i].dz,
                state->projectiles[i].active);
    }
    fprintf(file, "enemy_projectiles: count=%d\n", MAX_ENEMY_PROJECTILES);
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        fprintf(file, "enemy_projectile%d: x=%.3f y=%.3f z=%.3f dx=%.3f dy=%.3f dz=%.3f active=%d\n",
                i, state->enemy_projectiles[i].x, state->enemy_projectiles[i].y, state->enemy_projectiles[i].z,
                state->enemy_projectiles[i].dx, state->enemy_projectiles[i].dy, state->enemy_projectiles[i].dz,
                state->enemy_projectiles[i].active);
    }

    fclose(file);
    printf("Game saved to %s\n", filename);
}

void load_game(const char* filename, GameState* state) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open file %s for loading.\n", filename);
        return;
    }

    char line[256];
    int enemy_count, proj_count, enemy_proj_count;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "player:", 7) == 0) {
            sscanf(line, "player: x=%f y=%f z=%f health=%f yaw=%f pitch=%f",
                   &state->x, &state->y, &state->z, &state->health, &state->yaw, &state->pitch);
        } else if (strncmp(line, "enemies: count=", 15) == 0) {
            sscanf(line, "enemies: count=%d", &enemy_count);
        } else if (strncmp(line, "enemy", 5) == 0) {
            int idx;
            sscanf(line, "enemy%d: x=%f y=%f z=%f health=%f alive=%d",
                   &idx, &state->enemies[idx].x, &state->enemies[idx].y, &state->enemies[idx].z,
                   &state->enemies[idx].health, &state->enemies[idx].alive);
        } else if (strncmp(line, "projectiles: count=", 19) == 0) {
            sscanf(line, "projectiles: count=%d", &proj_count);
        } else if (strncmp(line, "projectile", 10) == 0) {
            int idx;
            sscanf(line, "projectile%d: x=%f y=%f z=%f dx=%f dy=%f dz=%f active=%d",
                   &idx, &state->projectiles[idx].x, &state->projectiles[idx].y, &state->projectiles[idx].z,
                   &state->projectiles[idx].dx, &state->projectiles[idx].dy, &state->projectiles[idx].dz,
                   &state->projectiles[idx].active);
        } else if (strncmp(line, "enemy_projectiles: count=", 25) == 0) {
            sscanf(line, "enemy_projectiles: count=%d", &enemy_proj_count);
        } else if (strncmp(line, "enemy_projectile", 16) == 0) {
            int idx;
            sscanf(line, "enemy_projectile%d: x=%f y=%f z=%f dx=%f dy=%f dz=%f active=%d",
                   &idx, &state->enemy_projectiles[idx].x, &state->enemy_projectiles[idx].y, &state->enemy_projectiles[idx].z,
                   &state->enemy_projectiles[idx].dx, &state->enemy_projectiles[idx].dy, &state->enemy_projectiles[idx].dz,
                   &state->enemy_projectiles[idx].active);
        }
    }

    fclose(file);
    printf("Game loaded from %s\n", filename);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    gluLookAt(x, y, z,
              x + front_x, y + front_y, z + front_z,
              0.0f, 1.0f, 0.0f);

    GLdouble modelview[16], projection[16];
    GLint viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    glColor3f(0.9f, 0.9f, 0.9f);
    glBegin(GL_QUADS);
    glVertex3f(-100.0f, 0.0f, -100.0f);
    glVertex3f(-100.0f, 0.0f, 100.0f);
    glVertex3f(100.0f, 0.0f, 100.0f);
    glVertex3f(100.0f, 0.0f, -100.0f);
    glEnd();

    glColor3f(1.0f, 0.0f, 0.0f);
    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (enemies[i].alive) {
            glPushMatrix();
            glTranslatef(enemies[i].x, enemies[i].y, enemies[i].z);
            glutSolidCube(1.0f);
            glPopMatrix();
        }
    }

    glColor3f(0.0f, 0.0f, 1.0f);
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) {
            glPushMatrix();
            glTranslatef(projectiles[i].x, projectiles[i].y, projectiles[i].z);
            glutSolidSphere(PROJECTILE_RADIUS, 16, 16);
            glPopMatrix();
        }
    }

    glColor3f(1.0f, 1.0f, 0.0f);
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        if (enemy_projectiles[i].active) {
            glPushMatrix();
            glTranslatef(enemy_projectiles[i].x, enemy_projectiles[i].y, enemy_projectiles[i].z);
            glutSolidSphere(PROJECTILE_RADIUS, 16, 16);
            glPopMatrix();
        }
    }

    draw_gun();
    draw_hud(modelview, projection, viewport);

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)w / (float)h, 0.1f, 200.0f);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int mx, int my) {
    float right_x, right_y, right_z;
    float norm;

    right_x = front_y * 0.0f - front_z * 1.0f;
    right_y = front_z * 0.0f - front_x * 0.0f;
    right_z = front_x * 1.0f - front_y * 0.0f;
    norm = sqrt(right_x * right_x + right_y * right_y + right_z * right_z);
    if (norm != 0.0f) {
        right_x /= norm;
        right_y /= norm;
        right_z /= norm;
    }

    switch (key) {
        case 'w':
            x += front_x * MOVE_SPEED;
            z += front_z * MOVE_SPEED;
            break;
        case 's':
            x -= front_x * MOVE_SPEED;
            z -= front_z * MOVE_SPEED;
            break;
        case 'a':
            x -= right_x * MOVE_SPEED;
            z -= right_z * MOVE_SPEED;
            break;
        case 'd':
            x += right_x * MOVE_SPEED;
            z += right_z * MOVE_SPEED;
            break;
        case 'q':
            yaw += 2.0f;
            update_camera_direction();
            break;
        case 'e':
            yaw -= 2.0f;
            update_camera_direction();
            break;
        case 'f':
            shoot();
            break;
        case 't':
            ascii_enabled = !ascii_enabled;
            printf("ASCII rendering %s\n", ascii_enabled ? "enabled" : "disabled");
            break;
        case 'p': {
            GameState state = {x, y, z, yaw, pitch, player_health};
            memcpy(state.enemies, enemies, sizeof(enemies));
            memcpy(state.projectiles, projectiles, sizeof(projectiles));
            memcpy(state.enemy_projectiles, enemy_projectiles, sizeof(enemy_projectiles));
            save_game("savegame.txt", &state);
            break;
        }
        case 'l': {
            GameState state;
            load_game("savegame.txt", &state);
            x = state.x;
            y = state.y;
            z = state.z;
            yaw = state.yaw;
            pitch = state.pitch;
            player_health = state.health;
            memcpy(enemies, state.enemies, sizeof(enemies));
            memcpy(projectiles, state.projectiles, sizeof(projectiles));
            memcpy(enemy_projectiles, state.enemy_projectiles, sizeof(enemy_projectiles));
            update_camera_direction();
            break;
        }
        case 27: // ESC
            exit(0);
            break;
    }
    glutPostRedisplay();
}

void special_keys(int key, int mx, int my) {
    float right_x, right_y, right_z;
    float norm;

    right_x = front_y * 0.0f - front_z * 1.0f;
    right_y = front_z * 0.0f - front_x * 0.0f;
    right_z = front_x * 1.0f - front_y * 0.0f;
    norm = sqrt(right_x * right_x + right_y * right_y + right_z * right_z);
    if (norm != 0.0f) {
        right_x /= norm;
        right_y /= norm;
        right_z /= norm;
    }

    switch (key) {
        case GLUT_KEY_UP:
            x += front_x * MOVE_SPEED;
            z += front_z * MOVE_SPEED;
            break;
        case GLUT_KEY_DOWN:
            x -= front_x * MOVE_SPEED;
            z -= front_z * MOVE_SPEED;
            break;
        case GLUT_KEY_LEFT:
            x -= right_x * MOVE_SPEED;
            z -= right_z * MOVE_SPEED;
            break;
        case GLUT_KEY_RIGHT:
            x += right_x * MOVE_SPEED;
            z += right_z * MOVE_SPEED;
            break;
    }
    glutPostRedisplay();
}

void passive_mouse_motion(int mx, int my) {
    if (mouse_first_entry) {
        mouse_last_y = my;
        mouse_first_entry = 0;
        return;
    }

    int dy = my - mouse_last_y;
    pitch -= dy * SENSITIVITY;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    update_camera_direction();
    mouse_last_y = my;

    glutPostRedisplay();
}

void shoot() {
    float right_x = front_y * 0.0f - front_z * 1.0f;
    float right_y = front_z * 0.0f - front_x * 0.0f;
    float right_z = front_x * 1.0f - front_y * 0.0f;
    float norm = sqrt(right_x * right_x + right_y * right_y + right_z * right_z);
    if (norm != 0.0f) {
        right_x /= norm;
        right_y /= norm;
        right_z /= norm;
    }

    float spawn_forward = 1.1f;
    float spawn_right = 0.3f;
    float spawn_down = -0.3f;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].x = x + front_x * spawn_forward + right_x * spawn_right;
            projectiles[i].y = y + front_y * spawn_forward + right_y * spawn_right + spawn_down;
            projectiles[i].z = z + front_z * spawn_forward + right_z * spawn_right;
            projectiles[i].dx = front_x * PROJECTILE_SPEED;
            projectiles[i].dy = front_y * PROJECTILE_SPEED;
            projectiles[i].dz = front_z * PROJECTILE_SPEED;
            projectiles[i].time_alive = 0.0f;
            projectiles[i].active = 1;
            break;
        }
    }
}

void enemy_shoot(int enemy_idx) {
    if (!enemies[enemy_idx].alive) return;

    float dx = x - enemies[enemy_idx].x;
    float dy = y - enemies[enemy_idx].y;
    float dz = z - enemies[enemy_idx].z;
    float dist = sqrt(dx * dx + dy * dy + dz * dz);
    if (dist == 0.0f) return;

    dx /= dist;
    dy /= dist;
    dz /= dist;

    float spawn_forward = 0.8f;
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        if (!enemy_projectiles[i].active) {
            enemy_projectiles[i].x = enemies[enemy_idx].x + dx * spawn_forward;
            enemy_projectiles[i].y = enemies[enemy_idx].y + dy * spawn_forward;
            enemy_projectiles[i].z = enemies[enemy_idx].z + dz * spawn_forward;
            enemy_projectiles[i].dx = dx * ENEMY_PROJECTILE_SPEED;
            enemy_projectiles[i].dy = dy * ENEMY_PROJECTILE_SPEED;
            enemy_projectiles[i].dz = dz * ENEMY_PROJECTILE_SPEED;
            enemy_projectiles[i].time_alive = 0.0f;
            enemy_projectiles[i].active = 1;
            enemies[enemy_idx].last_shot_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
            break;
        }
    }
}

void update_projectiles() {
    double current_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
    double delta_time = current_time - last_time;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) {
            projectiles[i].x += projectiles[i].dx * delta_time;
            projectiles[i].y += projectiles[i].dy * delta_time;
            projectiles[i].z += projectiles[i].dz * delta_time;
            projectiles[i].time_alive += delta_time;
            if (projectiles[i].time_alive > PROJECTILE_LIFETIME) {
                projectiles[i].active = 0;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        if (enemy_projectiles[i].active) {
            enemy_projectiles[i].x += enemy_projectiles[i].dx * delta_time;
            enemy_projectiles[i].y += enemy_projectiles[i].dy * delta_time;
            enemy_projectiles[i].z += enemy_projectiles[i].dz * delta_time;
            enemy_projectiles[i].time_alive += delta_time;
            if (enemy_projectiles[i].time_alive > PROJECTILE_LIFETIME) {
                enemy_projectiles[i].active = 0;
            }
        }
    }

    last_time = current_time;
}

void update_enemies() {
    double current_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0;

    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (!enemies[i].alive) continue;

        float dx = x - enemies[i].x;
        float dz = z - enemies[i].z;
        float dist = sqrt(dx * dx + dz * dz);
        if (dist > 1.0f) {
            dx /= dist;
            dz /= dist;
            enemies[i].x += dx * ENEMY_MOVE_SPEED;
            enemies[i].z += dz * ENEMY_MOVE_SPEED;
        }

        if (current_time - enemies[i].last_shot_time >= ENEMY_SHOOT_INTERVAL) {
            enemy_shoot(i);
        }
    }
}

void check_collisions() {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        for (int j = 0; j < NUM_ENEMIES; j++) {
            if (!enemies[j].alive) continue;

            float dx = projectiles[i].x - enemies[j].x;
            float dy = projectiles[i].y - enemies[j].y;
            float dz = projectiles[i].z - enemies[j].z;
            float distance = sqrt(dx * dx + dy * dy + dz * dz);
            float min_distance = PROJECTILE_RADIUS + enemies[j].radius;

            if (distance < min_distance) {
                enemies[j].health -= 1.0f;
                projectiles[i].active = 0;
                if (enemies[j].health <= 0.0f) {
                    enemies[j].alive = 0;
                }
                break;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        if (!enemy_projectiles[i].active) continue;

        float dx = enemy_projectiles[i].x - x;
        float dy = enemy_projectiles[i].y - y;
        float dz = enemy_projectiles[i].z - z;
        float distance = sqrt(dx * dx + dy * dy + dz * dz);
        float min_distance = PROJECTILE_RADIUS + PLAYER_RADIUS;

        if (distance < min_distance) {
            player_health -= 10.0f;
            enemy_projectiles[i].active = 0;
            if (player_health <= 0.0f) {
                exit(0);
            }
        }
    }
}

void idle() {
    update_projectiles();
    update_enemies();
    check_collisions();
    if (ascii_enabled) {
        draw_ascii();
    }
    glutPostRedisplay();
}

void init() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    update_camera_direction();

    enemies[0].x = 0.0f; enemies[0].y = 0.5f; enemies[0].z = -10.0f; enemies[0].radius = 0.7f; enemies[0].alive = 1; enemies[0].health = ENEMY_MAX_HEALTH; enemies[0].last_shot_time = 0.0;
    enemies[1].x = 5.0f; enemies[1].y = 0.5f; enemies[1].z = -15.0f; enemies[1].radius = 0.7f; enemies[1].alive = 1; enemies[1].health = ENEMY_MAX_HEALTH; enemies[1].last_shot_time = 0.0;
    enemies[2].x = -5.0f; enemies[2].y = 0.5f; enemies[2].z = -20.0f; enemies[2].radius = 0.7f; enemies[2].alive = 1; enemies[2].health = ENEMY_MAX_HEALTH; enemies[2].last_shot_time = 0.0;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = 0;
    }
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        enemy_projectiles[i].active = 0;
    }

    last_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("3D Shooter with Enemy AI and ASCII Rendering");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keys);
    glutPassiveMotionFunc(passive_mouse_motion);
    glutIdleFunc(idle);

    glutMainLoop();
    return 0;
}

