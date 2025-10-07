#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#define M_PI 3.14159265358979323846

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
    sprintf(outname, "data/%s_GEN.txt", ticker);

    FILE *out = fopen(outname, "w");
    if (!out) {
        perror("fopen output");
        return 1;
    }

    // Random initial price between 5.0 and 100.0
    double initial = 5.0 + (rand() / (double)RAND_MAX) * 95.0;

    // Randomly choose a "type" for variety in behavior, simulating different equity styles
    int type = rand() % 4;
    double a, b, std_res, target_mean = 0.0;
    if (type == 0) {  // Trending up (growth stock)
        a = 0.1 + (rand() / (double)RAND_MAX) * 0.2;  // Positive drift
        b = 1.0;
        std_res = 0.1 + (rand() / (double)RAND_MAX) * 0.3;  // Low to medium vol
    } else if (type == 1) {  // Trending down (declining stock)
        a = -0.3 + (rand() / (double)RAND_MAX) * 0.2;  // Negative drift
        b = 1.0;
        std_res = 0.1 + (rand() / (double)RAND_MAX) * 0.3;
    } else if (type == 2) {  // High volatility sideways (volatile penny stock)
        a = 0.0;
        b = 1.0;
        std_res = 0.5 + (rand() / (double)RAND_MAX) * 1.0;  // High vol
    } else {  // Mean-reverting (stable blue-chip around a mean)
        target_mean = 20.0 + (rand() / (double)RAND_MAX) * 80.0;
        b = 0.85 + (rand() / (double)RAND_MAX) * 0.1;  // <1 for reversion
        a = target_mean * (1.0 - b);
        std_res = 0.2 + (rand() / (double)RAND_MAX) * 0.4;
    }

    // Start with initial price
    double last = initial;
    fprintf(out, "%.1f\n", last);

    // Generate the remaining 49 prices
    for (int i = 1; i < 50; i++) {
        // Box-Muller for standard normal z
        double u1 = (double)rand() / RAND_MAX;
        double u2 = (double)rand() / RAND_MAX;
        double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);

        double noise = z * std_res;
        double new_val = a + b * last + noise;

        // Clip to positive for realism (stocks can't go negative)
        if (new_val < 0.1) new_val = 0.1;

        fprintf(out, "%.1f\n", new_val);
        last = new_val;
    }

    fclose(out);
    printf("Generated file: %s\n", outname);
    return 0;
}
