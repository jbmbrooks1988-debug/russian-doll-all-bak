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

    // Generate output filenames: TICKER_GEN.txt and TICKER_seed.txt
    char outname[20], seedname[20];
    sprintf(outname, "GEN/%s_GEN.txt", ticker);
    sprintf(seedname, "GEN/%s_seed.txt", ticker);

    FILE *out = fopen(outname, "w");
    if (!out) {
        perror("fopen output");
        return 1;
    }

    FILE *seed_out = fopen(seedname, "w");
    if (!seed_out) {
        perror("fopen seed");
        fclose(out);
        return 1;
    }

    // Random initial price log-uniform between 0.01 and 100000.0
    double log_min = log(0.01);
    double log_max = log(100000.0);
    double log_init = log_min + (rand() / (double)RAND_MAX) * (log_max - log_min);
    double initial = exp(log_init);

    // Randomly choose a "type" for variety
    int type = rand() % 5;
    char *trend_types[] = {"Growing", "Dying", "Volatile Sideways", "Mean-Reverting", "Cyclical"};
    double mu = 0.0, sigma = 0.0, kappa = 0.0, target_mean = 0.0, theta = 0.0;
    double mu_base = 0.0, amp_mu = 0.0;
    int period = 0;

    if (type == 0) {  // Growing - GBM with positive drift
        mu = 0.0001 + (rand() / (double)RAND_MAX) * 0.002;
        sigma = 0.005 + (rand() / (double)RAND_MAX) * 0.295;  // Wide vol 0.005-0.3
    } else if (type == 1) {  // Dying - GBM with negative drift
        mu = -0.003 + (rand() / (double)RAND_MAX) * 0.002;
        sigma = 0.005 + (rand() / (double)RAND_MAX) * 0.295;
    } else if (type == 2) {  // Volatile sideways - GBM high vol, mu=0
        mu = 0.0;
        sigma = 0.1 + (rand() / (double)RAND_MAX) * 0.3;  // Higher vol 0.1-0.4
    } else if (type == 3) {  // Mean-Reverting - Geometric OU
        double log_tmin = log(0.01);
        double log_tmax = log(100000.0);
        double log_target = log_tmin + (rand() / (double)RAND_MAX) * (log_tmax - log_tmin);
        target_mean = exp(log_target);
        theta = log(target_mean);
        kappa = 0.005 + (rand() / (double)RAND_MAX) * 0.045;  // Reversion 0.005-0.05
        sigma = 0.005 + (rand() / (double)RAND_MAX) * 0.295;
    } else if (type == 4) {  // Cyclical - GBM with sinusoidal mu
        mu_base = -0.001 + (rand() / (double)RAND_MAX) * 0.002;
        amp_mu = 0.0005 + (rand() / (double)RAND_MAX) * 0.0045;
        period = 10 + rand() % 31;  // 10-40
        sigma = 0.005 + (rand() / (double)RAND_MAX) * 0.295;
    }

    // Write seed file
    fprintf(seed_out, "Trend type: %s\nVolatility (sigma): %.4f\n", trend_types[type], sigma);
    if (type == 3) {
        fprintf(seed_out, "Target mean: %.2f\n", target_mean);
    } else if (type == 4) {
        fprintf(seed_out, "Mu base: %.4f\nMu amplitude: %.4f\nPeriod: %d\n", mu_base, amp_mu, period);
    }
    fclose(seed_out);

    // Start with initial price
    double last = initial;
    fprintf(out, "%.2f\n", last);

    // Generate the remaining 49 prices
    for (int i = 1; i < 50; i++) {
        double new_val;
        double z = gaussian_random();
        if (type == 3) {  // Geometric OU
            new_val = last * exp(kappa * (theta - log(last)) - 0.5 * sigma * sigma + sigma * z);
        } else if (type == 4) {  // Cyclical
            double phase = 2.0 * M_PI * i / period;
            double mu_i = mu_base + amp_mu * sin(phase);
            new_val = last * exp((mu_i - 0.5 * sigma * sigma) + sigma * z);
        } else {  // GBM for 0,1,2
            new_val = last * exp((mu - 0.5 * sigma * sigma) + sigma * z);
        }
        // Clip to 0.01 for realism (stocks can't go negative or zero)
        if (new_val < 0.01) new_val = 0.01;
        fprintf(out, "%.2f\n", new_val);
        last = new_val;
    }

    fclose(out);
    printf("Generated files: %s and %s\n", outname, seedname);
    return 0;
}
