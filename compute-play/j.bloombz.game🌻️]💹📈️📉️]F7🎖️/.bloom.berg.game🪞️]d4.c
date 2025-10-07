#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Hardcoded stock data: weekly adjusted closing prices (simplified real trends for Apple, BlackBerry, Tesla 2010-2015)
#define NUM_STOCKS 3
#define MAX_POINTS 50
float stocks[NUM_STOCKS][MAX_POINTS] = {
    {6.9, 7.1, 7.4, 7.2, 7.6, 7.9, 7.7, 8.1, 8.4, 8.7,  // AAPL rising
     8.9, 9.1, 9.4, 9.2, 9.6, 9.9, 9.7, 10.1, 10.4, 10.7,
     10.9, 11.1, 11.4, 11.2, 11.6, 11.9, 11.7, 12.1, 12.4, 12.7,
     12.9, 13.1, 13.4, 13.2, 13.6, 13.9, 13.7, 14.1, 14.4, 14.7,
     14.9, 15.1, 15.4, 15.2, 15.6, 15.9, 15.7, 16.1, 16.4, 16.7},
    {58.0, 57.5, 57.0, 56.5, 56.0, 55.5, 55.0, 54.5, 54.0, 53.5,  // BB falling
     53.0, 52.5, 52.0, 51.5, 51.0, 50.5, 50.0, 49.5, 49.0, 48.5,
     48.0, 47.5, 47.0, 46.5, 46.0, 45.5, 45.0, 44.5, 44.0, 43.5,
     43.0, 42.5, 42.0, 41.5, 41.0, 40.5, 40.0, 39.5, 39.0, 38.5,
     38.0, 37.5, 37.0, 36.5, 36.0, 35.5, 35.0, 34.5, 34.0, 33.5},
    {4.8, 5.0, 4.6, 5.3, 5.1, 5.5, 5.2, 5.8, 5.6, 6.0,  // TSLA volatile (adjusted for splits)
     5.7, 6.3, 6.1, 6.5, 6.2, 6.8, 6.6, 7.0, 6.7, 7.3,
     7.1, 7.5, 7.2, 7.8, 7.6, 8.0, 7.7, 8.3, 8.1, 8.5,
     8.2, 8.8, 8.6, 9.0, 8.7, 9.3, 9.1, 9.5, 9.2, 9.8,
     9.6, 10.0, 9.7, 10.3, 10.1, 10.5, 10.2, 10.8, 10.6, 11.0}
};
int data_lengths[NUM_STOCKS] = {50, 50, 50};
const char* stock_names[NUM_STOCKS] = {"Apple (AAPL)", "BlackBerry (BB)", "Tesla (TSLA)"};

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
void draw_chart(int stock_idx, int points_to_show);
void draw_button(const char* label, float x, float y, float w, float h);
void draw_text(const char* text, float x, float y);
float get_min_price(int stock_idx, int points);
float get_max_price(int stock_idx, int points);
float calculate_buy_hold(int stock_idx);
void timer(int value);

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
                    current_stock = rand() % NUM_STOCKS;
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

// Helper functions
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
    
    // Labels
    glColor3f(1.0, 1.0, 1.0);
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

int main(int argc, char** argv) {
    srand(time(NULL));
    current_stock = rand() % NUM_STOCKS;
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Refined Stock Trading Game Remake");
    
    glClearColor(0.0, 0.0, 0.0, 0.0);
    
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutReshapeFunc(reshape);
    
    glutMainLoop();
    return 0;
}
