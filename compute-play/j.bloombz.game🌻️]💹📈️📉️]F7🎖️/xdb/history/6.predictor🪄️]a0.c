#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <dirent.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MAX_POINTS 100
#define MAX_STOCKS 100
#define MAX_PATH 256
#define MAX_NAME 50
#define MAX_SEED_LINE 1024

// Stock data
float** stocks;
char** stock_names;
char** seed_data;
int* data_lengths;
int num_stocks = 0;

// Trade struct
typedef struct {
    int start_step;  // 1-based
    int end_step;    // -1 if open
    float start_price;
    float end_price;
} Trade;

// Game state for each stock
typedef struct {
    float cash;
    float shares;
    Trade trades[10];
    int num_trades;
    int buy_week;
    char trend_prediction[10];
} StockState;

// Global state
StockState* states;
int current_stock = 0;
int window_width = 800, window_height = 600;

// Forward declarations
void load_stock_data();
void simulate_trading(int stock_idx, StockState* state);
float compute_moving_average(int stock_idx, int week, int window);
void write_rounds_file();
void draw_chart(int stock_idx, int points_to_show, StockState* state);
void draw_text(const char* text, float x, float y);
float get_min_price(int stock_idx, int points);
float get_max_price(int stock_idx, int points);
void save_png(const char* ticker);
void cleanup();

void load_stock_data() {
    DIR* dir = opendir("GEN");
    if (!dir) {
        printf("Error: Could not open GEN/ directory\n");
        exit(1);
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "_GEN.txt")) num_stocks++;
    }
    rewinddir(dir);

    stocks = (float**)malloc(num_stocks * sizeof(float*));
    stock_names = (char**)malloc(num_stocks * sizeof(char*));
    data_lengths = (int*)malloc(num_stocks * sizeof(int));
    seed_data = (char**)malloc(num_stocks * sizeof(char*));
    states = (StockState*)malloc(num_stocks * sizeof(StockState));

    int i = 0;
    while ((entry = readdir(dir)) != NULL && i < num_stocks) {
        if (!strstr(entry->d_name, "_GEN.txt")) continue;

        char filepath[MAX_PATH];
        snprintf(filepath, MAX_PATH, "GEN/%s", entry->d_name);

        char* name = entry->d_name;
        char* dot = strstr(name, "_GEN.txt");
        stock_names[i] = (char*)malloc(MAX_NAME);
        strncpy(stock_names[i], name, dot - name);
        stock_names[i][dot - name] = '\0';

        FILE* ticker_file = fopen(filepath, "r");
        if (!ticker_file) {
            printf("Warning: Could not open %s\n", filepath);
            data_lengths[i] = 0;
            stocks[i] = NULL;
            seed_data[i] = NULL;
            states[i].cash = 1000.0;
            states[i].shares = 0.0;
            states[i].num_trades = 0;
            states[i].buy_week = -1;
            strcpy(states[i].trend_prediction, "None");
            i++;
            continue;
        }

        int count = 0;
        float price;
        while (fscanf(ticker_file, "%f", &price) == 1) {
            count++;
            char c;
            while ((c = fgetc(ticker_file)) != '\n' && c != EOF);
        }
        rewind(ticker_file);

        stocks[i] = (float*)malloc(count * sizeof(float));
        data_lengths[i] = count;
        int j = 0;
        while (j < count && fscanf(ticker_file, "%f", &stocks[i][j]) == 1) {
            char c;
            while ((c = fgetc(ticker_file)) != '\n' && c != EOF);
            j++;
        }
        fclose(ticker_file);

        char seed_filename[MAX_PATH];
        snprintf(seed_filename, MAX_PATH, "GEN/%s_seed.txt", stock_names[i]);
        FILE* seed_file = fopen(seed_filename, "r");
        seed_data[i] = (char*)malloc(MAX_SEED_LINE);
        seed_data[i][0] = '\0';
        if (seed_file) {
            char line[MAX_SEED_LINE];
            char seed_content[MAX_SEED_LINE] = "";
            while (fgets(line, sizeof(line), seed_file)) {
                strcat(seed_content, line);
            }
            strcpy(seed_data[i], seed_content);
            fclose(seed_file);
        } else {
            strcpy(seed_data[i], "Seed data not found\n");
        }

        states[i].cash = 1000.0;
        states[i].shares = 0.0;
        states[i].num_trades = 0;
        states[i].buy_week = -1;
        strcpy(states[i].trend_prediction, "None");
        i++;
    }
    closedir(dir);
}

float compute_moving_average(int stock_idx, int week, int window) {
    if (week < window) return stocks[stock_idx][week];
    float sum = 0.0;
    for (int i = week - window; i < week; i++) {
        sum += stocks[stock_idx][i];
    }
    return sum / window;
}

void simulate_trading(int stock_idx, StockState* state) {
    for (int week = 10; week < data_lengths[stock_idx]; week++) {
        float short_ma = compute_moving_average(stock_idx, week, 5);
        float long_ma = compute_moving_average(stock_idx, week, 10);
        float prev_short_ma = compute_moving_average(stock_idx, week - 1, 5);
        float prev_long_ma = compute_moving_average(stock_idx, week - 1, 10);

        if (state->shares == 0 && prev_short_ma <= prev_long_ma && short_ma > long_ma) {
            // Buy signal: short MA crosses above long MA
            state->shares = state->cash / stocks[stock_idx][week];
            state->cash = 0.0;
            Trade t = {week + 1, -1, stocks[stock_idx][week], 0.0};
            state->trades[state->num_trades++] = t;
            state->buy_week = week + 1;
            strcpy(state->trend_prediction, "Up");
        } else if (state->shares > 0 && prev_short_ma >= prev_long_ma && short_ma < long_ma) {
            // Sell signal: short MA crosses below long MA
            state->cash = state->shares * stocks[stock_idx][week];
            state->shares = 0.0;
            for (int i = state->num_trades - 1; i >= 0; i--) {
                if (state->trades[i].end_step == -1) {
                    state->trades[i].end_step = week + 1;
                    state->trades[i].end_price = stocks[stock_idx][week];
                    break;
                }
            }
        }
    }

    // Force sell at end if holding
    if (state->shares > 0) {
        state->cash = state->shares * stocks[stock_idx][data_lengths[stock_idx] - 1];
        state->shares = 0.0;
        for (int i = state->num_trades - 1; i >= 0; i--) {
            if (state->trades[i].end_step == -1) {
                state->trades[i].end_step = data_lengths[stock_idx];
                state->trades[i].end_price = stocks[stock_idx][data_lengths[stock_idx] - 1];
                break;
            }
        }
    }
}

void write_rounds_file() {
    FILE* fp = fopen("prediction/rounds.txt", "w");
    if (!fp) {
        printf("Error: Could not open prediction/rounds.txt\n");
        return;
    }
    fprintf(fp, "ticker,round,month_purchase,trend_prediction\n");
    for (int i = 0; i < num_stocks; i++) {
        if (data_lengths[i] == 0) continue;
        fprintf(fp, "%s,1,%d,%s\n", stock_names[i], states[i].buy_week, states[i].trend_prediction);
    }
    fclose(fp);
}

void draw_chart(int stock_idx, int points_to_show, StockState* state) {
    float min_p = get_min_price(stock_idx, points_to_show);
    float max_p = get_max_price(stock_idx, points_to_show);
    float price_range = max_p - min_p ? max_p - min_p : 1.0;
    float chart_width = window_width * 0.8;
    float chart_height = window_height * 0.6;
    float x_start = window_width * 0.1;
    float y_start = window_height * 0.3;

    glBegin(GL_LINES);
    glVertex2f(x_start, y_start);
    glVertex2f(x_start + chart_width, y_start);
    glVertex2f(x_start, y_start);
    glVertex2f(x_start, y_start + chart_height);
    glEnd();

    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < points_to_show; i++) {
        float x = x_start + (chart_width / (points_to_show - 1.0)) * i;
        float y = y_start + ((stocks[stock_idx][i] - min_p) / price_range) * chart_height;
        glVertex2f(x, y);
    }
    glEnd();

    for (int i = 0; i < state->num_trades; i++) {
        int start = state->trades[i].start_step - 1;
        int end = state->trades[i].end_step == -1 ? points_to_show - 1 : state->trades[i].end_step - 1;
        float end_p = state->trades[i].end_step == -1 ? stocks[stock_idx][points_to_show - 1] : state->trades[i].end_price;
        float profit = end_p - state->trades[i].start_price;
        glColor3f(profit > 0 ? 0.0 : 1.0, profit > 0 ? 1.0 : 0.0, 0.0);
        float start_x = x_start + (chart_width / (points_to_show - 1.0)) * start;
        float end_x = x_start + (chart_width / (points_to_show - 1.0)) * end;
        float start_y = y_start + ((state->trades[i].start_price - min_p) / price_range) * chart_height;
        float end_y = y_start + ((end_p - min_p) / price_range) * chart_height;
        glBegin(GL_LINES);
        glVertex2f(start_x, start_y);
        glVertex2f(end_x, end_y);
        glEnd();
    }

    glColor3f(1.0, 1.0, 1.0);
    char price_text[50];
    float label_x = x_start + chart_width + 10;
    int num_ticks = 5;
    for (int i = 0; i < num_ticks; i++) {
        float fraction = (float)i / (num_ticks - 1);
        float tick_price = min_p + fraction * price_range;
        float tick_y = y_start + fraction * chart_height;
        glBegin(GL_LINES);
        glVertex2f(x_start + chart_width, tick_y);
        glVertex2f(x_start + chart_width + 5, tick_y);
        glEnd();
        sprintf(price_text, "$%.2f", tick_price);
        draw_text(price_text, label_x, tick_y + (i == 0 ? 10 : -5));
    }

    float current_price = stocks[stock_idx][points_to_show - 1];
    float current_y = y_start + ((current_price - min_p) / price_range) * chart_height;
    bool draw_current = true;
    float overlap_threshold = 15.0;
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
        draw_text(price_text, label_x, current_y - 5);
    }

    char week_text[50];
    sprintf(week_text, "Week #%d", points_to_show);
    draw_text(week_text, x_start + chart_width / 2 - 50, y_start - 20);
    draw_text("Price", x_start - 30, y_start + chart_height / 2);

    char stock_text[50];
    sprintf(stock_text, "Stock: %s", stock_names[stock_idx]);
    draw_text(stock_text, x_start, y_start + chart_height + 20);

    float portfolio = state->cash + state->shares * current_price;
    sprintf(price_text, "Final: $%.2f", portfolio);
    draw_text(price_text, x_start, 100);
    draw_text(seed_data[stock_idx], x_start, 80);
    sprintf(price_text, "Prediction: %s at week %d", state->trend_prediction, state->buy_week);
    draw_text(price_text, x_start, 60);
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

void save_png(const char* ticker) {
    char filename[MAX_PATH];
    snprintf(filename, MAX_PATH, "prediction/prediction_%s.png", ticker);
    unsigned char* pixels = (unsigned char*)malloc(window_width * window_height * 3);
    glReadPixels(0, 0, window_width, window_height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    stbi_flip_vertically_on_write(1);
    stbi_write_png(filename, window_width, window_height, 3, pixels, window_width * 3);
    free(pixels);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    if (data_lengths[current_stock] > 0) {
        draw_chart(current_stock, data_lengths[current_stock], &states[current_stock]);
        save_png(stock_names[current_stock]);
    }
    glutSwapBuffers();
    current_stock++;
    if (current_stock < num_stocks) {
        glutPostRedisplay();
    } else {
        glutLeaveMainLoop();
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

void cleanup() {
    for (int i = 0; i < num_stocks; i++) {
        if (stocks[i]) free(stocks[i]);
        if (stock_names[i]) free(stock_names[i]);
        if (seed_data[i]) free(seed_data[i]);
    }
    free(stocks);
    free(stock_names);
    free(data_lengths);
    free(seed_data);
    free(states);
}

int main(int argc, char** argv) {
    srand(time(NULL));
    load_stock_data();
    if (num_stocks == 0) {
        printf("Error: No valid stock data loaded\n");
        return 1;
    }

    system("mkdir -p prediction");
    for (int i = 0; i < num_stocks; i++) {
        if (data_lengths[i] > 0) simulate_trading(i, &states[i]);
    }
    write_rounds_file();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Stock Prediction Charts");
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    atexit(cleanup);
    glutMainLoop();
    return 0;
}
