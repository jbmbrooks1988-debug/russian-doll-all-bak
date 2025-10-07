#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>  // Added for bool type
#include <dirent.h>   // Added for directory scanning

// Constants
#define MAX_POINTS 100
#define MAX_STOCKS 100
#define MAX_PATH 256
#define MAX_NAME 50

// Stock data
float** stocks; // Dynamic array of stock prices
char** stock_names; // Dynamic array of stock names
int* data_lengths; // Array of data lengths
int num_stocks = 0;

// Trade struct for historic lines
typedef struct {
    int start_step;
    int end_step;  // -1 if open
    float start_price;
    float end_price;  // updated if closed
} Trade;
Trade trades[100];
int num_trades = 0;

// Game state
int current_stock = 0;
int current_step = 0;
int animation_running = 0;
int full_revealed = 0;
float cash = 1000.0;
float shares = 0.0;
int rounds_played = 0;
float initial_cash = 1000.0;
int window_width = 800, window_height = 600;

// Forward declarations
void load_stock_data();
void draw_chart(int stock_idx, int points_to_show);
void draw_button(const char* label, float x, float y, float w, float h);
void draw_text(const char* text, float x, float y);
float get_min_price(int stock_idx, int points);
float get_max_price(int stock_idx, int points);
float calculate_buy_hold(int stock_idx);
void timer(int value);
void cleanup();

// Load stock data from locations.txt or GEN/ directory
void load_stock_data() {
    FILE* loc_file = fopen("locations.txt", "r");
    char* file_list = NULL;
    int file_list_size = 0;
    bool use_locations = true;

    // Try to read locations.txt
    if (loc_file) {
        char line[1024];
        if (fgets(line, sizeof(line), loc_file)) {
            file_list_size = strlen(line) + 1;
            file_list = (char*)malloc(file_list_size);
            strcpy(file_list, line);
        } else {
            use_locations = false;  // Empty file
        }
        fclose(loc_file);
    } else {
        use_locations = false;  // File not found
    }

    // If locations.txt is not available or empty, scan GEN/ directory
    if (!use_locations) {
        DIR* dir = opendir("GEN");
        if (!dir) {
            printf("Error: Could not open GEN/ directory\n");
            exit(1);
        }

        // Count _GEN.txt files
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, "_GEN.txt")) {
                num_stocks++;
            }
        }
        rewinddir(dir);

        // Allocate memory for file paths
        file_list_size = num_stocks * MAX_PATH;
        file_list = (char*)malloc(file_list_size);
        file_list[0] = '\0';
        char* ptr = file_list;

        // Build file list
        int i = 0;
        while ((entry = readdir(dir)) != NULL && i < num_stocks) {
            if (strstr(entry->d_name, "_GEN.txt")) {
                snprintf(ptr, MAX_PATH, "GEN/%s ", entry->d_name);
                ptr += strlen(ptr);
                i++;
            }
        }
        closedir(dir);
    }

    if (num_stocks == 0) {
        printf("Error: No valid stock files found\n");
        free(file_list);
        exit(1);
    }

    // Allocate memory for stocks, names, and lengths
    stocks = (float**)malloc(num_stocks * sizeof(float*));
    stock_names = (char**)malloc(num_stocks * sizeof(char*));
    data_lengths = (int*)malloc(num_stocks * sizeof(int));

    // Parse file paths and load data
    char* token = strtok(file_list, " \n");
    int i = 0;
    while (token && i < num_stocks) {
        // Extract name from path (strip _GEN.txt and directories)
        char* name = strrchr(token, '/');
        if (!name) name = token; else name++;
        char* dot = strstr(name, "_GEN.txt");
        stock_names[i] = (char*)malloc(MAX_NAME);
        if (dot) strncpy(stock_names[i], name, dot - name);
        else strcpy(stock_names[i], name);
        stock_names[i][dot ? dot - name : strlen(name)] = '\0';

        // Read ticker file
        FILE* ticker_file = fopen(token, "r");
        if (!ticker_file) {
            printf("Warning: Could not open %s\n", token);
            data_lengths[i] = 0;
            stocks[i] = NULL;
            i++;
            token = strtok(NULL, " \n");
            continue;
        }

        // Count lines
        int count = 0;
        float price;
        while (fscanf(ticker_file, "%f", &price) == 1) {
            count++;
            char c;
            while ((c = fgetc(ticker_file)) != '\n' && c != EOF);
        }
        rewind(ticker_file);

        // Allocate and load prices
        stocks[i] = (float*)malloc(count * sizeof(float));
        data_lengths[i] = count;
        int j = 0;
        while (j < count && fscanf(ticker_file, "%f", &stocks[i][j]) == 1) {
            char c;
            while ((c = fgetc(ticker_file)) != '\n' && c != EOF);
            j++;
        }
        fclose(ticker_file);
        i++;
        token = strtok(NULL, " \n");
    }

    free(file_list);
}

// GLUT callbacks
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1.0, 1.0, 1.0);
    
    // Always draw portfolio value
    float portfolio = cash;
    if (animation_running || full_revealed) {
        float current_price = stocks[current_stock][current_step - 1];
        portfolio = cash + shares * current_price;
    }
    char port_text[50];
    sprintf(port_text, "Portfolio: $%.2f", portfolio);
    draw_text(port_text, window_width - 200, window_height - 50);
    
    if (animation_running || full_revealed) {
        // Draw chart
        draw_chart(current_stock, current_step);
        
        // Draw time label
        char time_text[50];
        sprintf(time_text, "Week: %d", current_step);
        draw_text(time_text, window_width / 2 - 50, 50);
    }
    
    if (!animation_running && !full_revealed) {
        draw_button("Start", 300, 250, 200, 50);
    } else if (full_revealed) {
        // Show results
        char result[100];
        sprintf(result, "Final: $%.2f | Buy-Hold: $%.2f | Stock: %s", portfolio, calculate_buy_hold(current_stock), stock_names[current_stock]);
        draw_text(result, 100, 100);
        draw_button("Next Round", 500, 50, 200, 50);
    }
    
    glutSwapBuffers();
}

void mouse(int button, int state, int x, int y) {
    y = window_height - y;
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            // Check for buttons
            if (!animation_running && !full_revealed) {
                if (x >= 300 && x <= 500 && y >= 250 && y <= 300) {
                    animation_running = 1;
                    current_step = 1;
                    cash = initial_cash;
                    shares = 0.0;
                    num_trades = 0;
                    timer(0);
                }
            } else if (full_revealed) {
                if (x >= 500 && x <= 700 && y >= 50 && y <= 100) {
                    full_revealed = 0;
                    animation_running = 0;
                    current_stock = rand() % num_stocks;
                    while (data_lengths[current_stock] == 0) {
                        current_stock = rand() % num_stocks;
                    }
                    rounds_played++;
                }
            } else if (animation_running && shares == 0 && current_step > 0) {
                // Buy
                float current_price = stocks[current_stock][current_step - 1];
                shares = cash / current_price;
                cash = 0.0;
                Trade t = {current_step, -1, current_price, 0.0};
                trades[num_trades++] = t;
            }
        } else if (state == GLUT_UP && animation_running && shares > 0) {
            // Sell
            float current_price = stocks[current_stock][current_step - 1];
            cash = shares * current_price;
            shares = 0.0;
            for (int i = num_trades - 1; i >= 0; i--) {
                if (trades[i].end_step == -1) {
                    trades[i].end_step = current_step;
                    trades[i].end_price = current_price;
                    break;
                }
            }
        }
        glutPostRedisplay();
    }
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
}

void timer(int value) {
    if (animation_running && current_step < data_lengths[current_stock]) {
        current_step++;
        glutPostRedisplay();
        glutTimerFunc(200, timer, 0);
    } else if (current_step >= data_lengths[current_stock]) {
        animation_running = 0;
        full_revealed = 1;
        // Force sell if holding
        if (shares > 0) {
            float current_price = stocks[current_stock][data_lengths[current_stock] - 1];
            cash = shares * current_price;
            shares = 0.0;
            for (int i = num_trades - 1; i >= 0; i--) {
                if (trades[i].end_step == -1) {
                    trades[i].end_step = current_step;
                    trades[i].end_price = current_price;
                    break;
                }
            }
        }
        glutPostRedisplay();
    }
}

void draw_chart(int stock_idx, int points_to_show) {
    float min_p = get_min_price(stock_idx, points_to_show);
    float max_p = get_max_price(stock_idx, points_to_show);
    float price_range = max_p - min_p ? max_p - min_p : 1.0;
    float chart_width = window_width * 0.8;
    float chart_height = window_height * 0.6;
    float x_start = window_width * 0.1;
    float y_start = window_height * 0.3;
    
    // Draw axes
    glBegin(GL_LINES);
    glVertex2f(x_start, y_start);
    glVertex2f(x_start + chart_width, y_start);
    glVertex2f(x_start, y_start);
    glVertex2f(x_start, y_start + chart_height);
    glEnd();
    
    // Draw price line (white)
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < points_to_show; i++) {
        float x = x_start + (chart_width / (points_to_show - 1.0)) * i;
        float y = y_start + ((stocks[stock_idx][i] - min_p) / price_range) * chart_height;
        glVertex2f(x, y);
    }
    glEnd();
    
    // Draw trade lines (historic and dynamic)
    for (int i = 0; i < num_trades; i++) {
        int start = trades[i].start_step - 1;  // 0-based
        int end = trades[i].end_step == -1 ? points_to_show - 1 : trades[i].end_step - 1;
        float end_p = trades[i].end_step == -1 ? stocks[stock_idx][points_to_show - 1] : trades[i].end_price;
        float profit = end_p - trades[i].start_price;
        glColor3f(profit > 0 ? 0.0 : 1.0, profit > 0 ? 1.0 : 0.0, 0.0);  // Green or red
        float start_x = x_start + (chart_width / (points_to_show - 1.0)) * start;
        float end_x = x_start + (chart_width / (points_to_show - 1.0)) * end;
        float start_y = y_start + ((trades[i].start_price - min_p) / price_range) * chart_height;
        float end_y = y_start + ((end_p - min_p) / price_range) * chart_height;
        glBegin(GL_LINES);
        glVertex2f(start_x, start_y);
        glVertex2f(end_x, end_y);
        glEnd();
    }
    
    // Draw y-axis tick labels on the right (mid-ranges including high and low)
    glColor3f(1.0, 1.0, 1.0);
    char price_text[50];
    float label_x = x_start + chart_width + 10;  // Position to the right of the chart
    int num_ticks = 5;  // Includes low, mids, and high
    for (int i = 0; i < num_ticks; i++) {
        float fraction = (float)i / (num_ticks - 1);
        float tick_price = min_p + fraction * price_range;
        float tick_y = y_start + fraction * chart_height;
        
        // Optional small tick mark
        glBegin(GL_LINES);
        glVertex2f(x_start + chart_width, tick_y);
        glVertex2f(x_start + chart_width + 5, tick_y);
        glEnd();
        
        // Draw tick label (slightly offset for bottom visibility)
        sprintf(price_text, "$%.2f", tick_price);
        draw_text(price_text, label_x, tick_y + (i == 0 ? 10 : -5));  // Offset bottom up, others down for centering
    }
    
    // Current price label (only if not overlapping/too close to a tick)
    float current_price = stocks[stock_idx][points_to_show - 1];
    float current_y = y_start + ((current_price - min_p) / price_range) * chart_height;
    bool draw_current = true;
    float overlap_threshold = 15.0;  // Pixels; approximate text height
    for (int i = 0; i < num_ticks; i++) {
        float fraction = (float)i / (num_ticks - 1);
        float tick_y = y_start + fraction * chart_height;
        if (fabs(current_y - tick_y) < overlap_threshold) {
            draw_current = false;
            break;
        }
    }
    if (draw_current) {
        sprintf(price_text, "$%.2f", current_price);
        draw_text(price_text, label_x, current_y - 5);  // Slight offset for centering
    }
    
    // Labels for axes
    draw_text("Time (Weeks)", x_start + chart_width / 2 - 50, y_start - 20);
    draw_text("Price", x_start - 30, y_start + chart_height / 2);
}

void draw_button(const char* label, float x, float y, float w, float h) {
    glColor3f(0.5, 0.5, 0.5);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    glColor3f(1.0, 1.0, 1.0);
    draw_text(label, x + 10, y + 20);
}

void draw_text(const char* text, float x, float y) {
    glRasterPos2f(x, y);
    for (int i = 0; i < strlen(text); i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, text[i]);
    }
}

float get_min_price(int stock_idx, int points) {
    float min_p = stocks[stock_idx][0];
    for (int i = 1; i < points; i++) {
        if (stocks[stock_idx][i] < min_p) min_p = stocks[stock_idx][i];
    }
    return min_p;
}

float get_max_price(int stock_idx, int points) {
    float max_p = stocks[stock_idx][0];
    for (int i = 1; i < points; i++) {
        if (stocks[stock_idx][i] > max_p) max_p = stocks[stock_idx][i];
    }
    return max_p;
}

float calculate_buy_hold(int stock_idx) {
    float first_price = stocks[stock_idx][0];
    float last_price = stocks[stock_idx][data_lengths[stock_idx] - 1];
    return initial_cash * (last_price / first_price);
}

void cleanup() {
    for (int i = 0; i < num_stocks; i++) {
        if (stocks[i]) free(stocks[i]);
        if (stock_names[i]) free(stock_names[i]);
    }
    free(stocks);
    free(stock_names);
    free(data_lengths);
}

int main(int argc, char** argv) {
    srand(time(NULL));
    load_stock_data();
    
    if (num_stocks == 0) {
        printf("Error: No valid stock data loaded\n");
        return 1;
    }
    
    current_stock = rand() % num_stocks;
    while (data_lengths[current_stock] == 0) {
        current_stock = rand() % num_stocks;
    }
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Refined Stock Trading Game Remake");
    
    glClearColor(0.0, 0.0, 0.0, 0.0);
    
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutReshapeFunc(reshape);
    
    atexit(cleanup);
    glutMainLoop();
    return 0;
}
