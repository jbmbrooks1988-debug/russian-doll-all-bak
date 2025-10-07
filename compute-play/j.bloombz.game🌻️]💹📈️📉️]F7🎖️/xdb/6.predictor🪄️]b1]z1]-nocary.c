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
#define NUM_ROUNDS 4
#define INITIAL_CASH 1000.0

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

// Prediction per week
typedef struct {
    int week;
    char trend_prediction[10];
} Prediction;

// Game state for each stock
typedef struct {
    float cash;
    float shares;
    Trade trades[10];
    int num_trades;
    Prediction predictions[MAX_POINTS];
    float round_percent_gain_loss[NUM_ROUNDS];
} StockState;

// Global state
StockState* states;
int current_stock = 0;
int window_width = 800, window_height = 600;

// Round boundaries
const int round_starts[] = {1, 14, 27, 40};
const int round_ends[] = {13, 26, 39, 50};

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
int compare_floats(const void* a, const void* b);

// Comparison function for qsort
int compare_floats(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

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
            states[i].cash = INITIAL_CASH;
            states[i].shares = 0.0;
            states[i].num_trades = 0;
            for (int j = 0; j < MAX_POINTS; j++) {
                states[i].predictions[j].week = j + 1;
                strcpy(states[i].predictions[j].trend_prediction, "None");
            }
            for (int j = 0; j < NUM_ROUNDS; j++) {
                states[i].round_percent_gain_loss[j] = 0.0;
            }
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

        states[i].cash = INITIAL_CASH;
        states[i].shares = 0.0;
        states[i].num_trades = 0;
        for (int j = 0; j < MAX_POINTS; j++) {
            states[i].predictions[j].week = j + 1;
            strcpy(states[i].predictions[j].trend_prediction, "None");
        }
        for (int j = 0; j < NUM_ROUNDS; j++) {
            states[i].round_percent_gain_loss[j] = 0.0;
        }
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
    float initial_round_cash = INITIAL_CASH; // For calculating round-specific gain/loss
    state->cash = INITIAL_CASH; // Initialize cash for the first round
    for (int r = 0; r < NUM_ROUNDS; r++) {
        float round_start_cash = state->cash; // Record cash at start of round
        state->shares = 0.0; // Reset shares at start of round
        int start_week = round_starts[r] - 1;
        int end_week = round_ends[r] - 1;

        for (int week = start_week; week <= end_week && week < data_lengths[stock_idx]; week++) {
            if (week >= 9) { // Need enough data for 10-week MA
                float short_ma = compute_moving_average(stock_idx, week, 5);
                float long_ma = compute_moving_average(stock_idx, week, 10);
                float prev_short_ma = compute_moving_average(stock_idx, week - 1, 5);
                float prev_long_ma = compute_moving_average(stock_idx, week - 1, 10);

                if (state->shares == 0 && prev_short_ma <= prev_long_ma && short_ma > long_ma) {
                    state->shares = state->cash / stocks[stock_idx][week];
                    state->cash = 0.0;
                    Trade t = {week + 1, -1, stocks[stock_idx][week], 0.0};
                    state->trades[state->num_trades++] = t;
                    strcpy(state->predictions[week].trend_prediction, "Up");
                } else if (state->shares > 0 && prev_short_ma >= prev_long_ma && short_ma < long_ma) {
                    state->cash = state->shares * stocks[stock_idx][week];
                    state->shares = 0.0;
                    for (int i = state->num_trades - 1; i >= 0; i--) {
                        if (state->trades[i].end_step == -1) {
                            state->trades[i].end_step = week + 1;
                            state->trades[i].end_price = stocks[stock_idx][week];
                            break;
                        }
                    }
                    strcpy(state->predictions[week].trend_prediction, "Down");
                } else {
                    strcpy(state->predictions[week].trend_prediction, "Hold");
                }
            } else {
                strcpy(state->predictions[week].trend_prediction, "Hold");
            }

            // Calculate round percent gain/loss based on round start cash
            float portfolio = state->cash + state->shares * stocks[stock_idx][week];
            state->round_percent_gain_loss[r] = (portfolio - round_start_cash) / round_start_cash * 100.0;
        }

        // Force sell at end of round if holding
        if (state->shares > 0 && end_week < data_lengths[stock_idx]) {
            state->cash = state->shares * stocks[stock_idx][end_week];
            state->shares = 0.0;
            for (int i = state->num_trades - 1; i >= 0; i--) {
                if (state->trades[i].end_step == -1) {
                    state->trades[i].end_step = end_week + 1;
                    state->trades[i].end_price = stocks[stock_idx][end_week];
                    break;
                }
            }
            strcpy(state->predictions[end_week].trend_prediction, "Down");
            float portfolio = state->cash;
            state->round_percent_gain_loss[r] = (portfolio - round_start_cash) / round_start_cash * 100.0;
        }
    }
}

void write_rounds_file() {
    FILE* fp = fopen("prediction/rounds.txt", "w");
    if (!fp) {
        printf("Error: Could not open prediction/rounds.txt\n");
        return;
    }
    fprintf(fp, "ticker,round,week,trend_prediction,round_percent_gain_loss\n");
    for (int i = 0; i < num_stocks; i++) {
        if (data_lengths[i] == 0) continue;
        for (int week = 0; week < data_lengths[i]; week++) {
            int round = 0;
            for (int r = 0; r < NUM_ROUNDS; r++) {
                if (week + 1 >= round_starts[r] && week + 1 <= round_ends[r]) {
                    round = r + 1;
                    break;
                }
            }
            fprintf(fp, "%s,%d,%d,%s,%.2f\n", stock_names[i], round, week + 1, 
                    states[i].predictions[week].trend_prediction, 
                    round > 0 ? states[i].round_percent_gain_loss[round - 1] : 0.0);
        }
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
        float price = stocks[stock_idx][i];
        // Clamp prices to the percentile range to avoid extreme values skewing the display
        if (price < min_p) price = min_p;
        if (price > max_p) price = max_p;
        float y = y_start + ((price - min_p) / price_range) * chart_height;
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
        float start_price = state->trades[i].start_price;
        float end_price = end_p;
        // Clamp trade prices to the percentile range
        if (start_price < min_p) start_price = min_p;
        if (start_price > max_p) start_price = max_p;
        if (end_price < min_p) end_price = min_p;
        if (end_price > max_p) end_price = max_p;
        float start_y = y_start + ((start_price - min_p) / price_range) * chart_height;
        float end_y = y_start + ((end_price - min_p) / price_range) * chart_height;
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
    sprintf(price_text, "Starting: $%.2f | Final: $%.2f", INITIAL_CASH, portfolio);
    draw_text(price_text, x_start, 100);
    draw_text(seed_data[stock_idx], x_start, 80);

    // Display round percent gain/loss
    for (int r = 0; r < NUM_ROUNDS; r++) {
        sprintf(price_text, "Round %d Gain/Loss: %.2f%%", r + 1, state->round_percent_gain_loss[r]);
        draw_text(price_text, x_start, 60 - r * 20);
    }

    // Display overall percent gain/loss
    float overall_percent = (portfolio - INITIAL_CASH) / INITIAL_CASH * 100.0;
    sprintf(price_text, "Overall Gain/Loss: %.2f%%", overall_percent);
    draw_text(price_text, x_start, 60 - NUM_ROUNDS * 20);
}

void draw_text(const char* text, float x, float y) {
    glRasterPos2f(x, y);
    for (int i = 0; i < strlen(text); i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, text[i]);
    }
}

float get_min_price(int stock_idx, int points) {
    // Copy prices to a temporary array for sorting
    float* temp_prices = (float*)malloc(points * sizeof(float));
    for (int i = 0; i < points; i++) {
        temp_prices[i] = stocks[stock_idx][i];
    }
    
    // Sort the prices
    qsort(temp_prices, points, sizeof(float), compare_floats);
    
    // Use the 5th percentile as the minimum price
    int index = (int)(0.05 * points);
    if (index < 0) index = 0;
    float min_p = temp_prices[index];
    
    free(temp_prices);
    return min_p;
}

float get_max_price(int stock_idx, int points) {
    // Copy prices to a temporary array for sorting
    float* temp_prices = (float*)malloc(points * sizeof(float));
    for (int i = 0; i < points; i++) {
        temp_prices[i] = stocks[stock_idx][i];
    }
    
    // Sort the prices
    qsort(temp_prices, points, sizeof(float), compare_floats);
    
    // Use the 95th percentile as the maximum price
    int index = (int)(0.95 * points);
    if (index >= points) index = points - 1;
    float max_p = temp_prices[index];
    
    free(temp_prices);
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
