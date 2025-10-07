#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#define M_PI 3.14159265358979323846

double gaussian_random() {
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

int main() {
    srand(time(NULL));

    // Generate random 4-letter ticker
    char ticker[5];
    for (int i = 0; i < 4; i++) {
        ticker[i] = 'A' + rand() % 26;
    }
    ticker[4] = '\0';

    // Generate output filename: TICKER_GEN.txt
    char outname[20];
    sprintf(outname, "%s_GEN.txt", ticker);

    FILE *out = fopen(outname, "w");
    if (!out) {
        perror("fopen output");
        return 1;
    }

    // Random initial price between 5.0 and 100.0
    double initial = 5.0 + (rand() / (double)RAND_MAX) * 95.0;

    // Randomly choose a "type" for variety in behavior, simulating different equity styles
    int type = rand() % 4;
    double mu = 0.0, sigma = 0.0, theta = 0.0, target_mean = 0.0;
    if (type == 0) {  // Trending up (growth stock) - GBM with positive drift
        mu = 0.0005 + (rand() / (double)RAND_MAX) * 0.001;  // Daily drift ~0.1-0.3% annual
        sigma = 0.01 + (rand() / (double)RAND_MAX) * 0.02;   // Volatility 1-3%
    } else if (type == 1) {  // Trending down (declining stock) - GBM with negative drift
        mu = -0.001 + (rand() / (double)RAND_MAX) * 0.0005; 
        sigma = 0.01 + (rand() / (double)RAND_MAX) * 0.02;
    } else if (type == 2) {  // High volatility sideways (volatile penny stock) - GBM high vol
        mu = 0.0;
        sigma = 0.05 + (rand() / (double)RAND_MAX) * 0.1;    // High vol 5-15%
    } else {  // Mean-reverting (stable blue-chip) - Ornstein-Uhlenbeck process
        target_mean = 20.0 + (rand() / (double)RAND_MAX) * 80.0;
        theta = 0.01 + (rand() / (double)RAND_MAX) * 0.05;   // Reversion speed
        sigma = 0.5 + (rand() / (double)RAND_MAX) * 1.0;     // Adjusted for OU vol
    }

    // Start with initial price
    double last = initial;
    fprintf(out, "%.2f\n", last);  // Use 2 decimals for realism

    // Generate the remaining 49 prices
    for (int i = 1; i < 50; i++) {
        double new_val;
        if (type == 3) {  // OU for mean-reverting
            double z = gaussian_random();
            new_val = last + theta * (target_mean - last) + sigma * z;
            if (new_val < 0.1) new_val = 0.1;  // Clip to positive
        } else {  // GBM for others
            double z = gaussian_random();
            new_val = last * exp((mu - 0.5 * sigma * sigma) + sigma * z);
        }
        fprintf(out, "%.2f\n", new_val);
        last = new_val;
    }

    fclose(out);
    printf("Generated file: %s\n", outname);
    return 0;
}
