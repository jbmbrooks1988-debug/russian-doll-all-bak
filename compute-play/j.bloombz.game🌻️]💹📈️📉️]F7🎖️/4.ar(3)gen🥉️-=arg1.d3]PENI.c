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

    // Generate output filenames
    char outname[30], seedname[30];
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

    // Randomly choose AR order (2 or 3)
    int ar_order = (rand() % 2) + 2;  // 2 or 3
    double phi[3] = {0.0, 0.0, 0.0};
    if (ar_order == 2) {
        phi[0] = 0.5 + (rand() / (double)RAND_MAX) * 0.4;  // phi1 in [0.5, 0.9]
        phi[1] = -0.3 + (rand() / (double)RAND_MAX) * 0.4; // phi2 in [-0.3, 0.1]
        if (fabs(phi[0] + phi[1]) > 0.99) phi[1] *= 0.5;   // Ensure stationarity
    } else {
        phi[0] = 0.4 + (rand() / (double)RAND_MAX) * 0.3;  // phi1 in [0.4, 0.7]
        phi[1] = -0.2 + (rand() / (double)RAND_MAX) * 0.3; // phi2 in [-0.2, 0.1]
        phi[2] = -0.2 + (rand() / (double)RAND_MAX) * 0.3; // phi3 in [-0.2, 0.1]
        if (phi[0] + phi[1] + phi[2] > 0.99) phi[2] *= 0.5;
    }

    // Randomly choose a trend type
    int type = rand() % 5;
    char *trend_types[] = {"Growing", "Dying", "Volatile Sideways", "Mean-Reverting", "Cyclical"};
    double mu = 0.0, sigma = 0.0, kappa = 0.0, target_mean = 0.0, theta = 0.0;
    double mu_base = 0.0, amp_mu = 0.0;
    int period = 0;

    if (type == 0) {  // Growing - AR with positive drift
        mu = 0.0001 + (rand() / (double)RAND_MAX) * 0.002;
        sigma = 0.005 + (rand() / (double)RAND_MAX) * 0.295;
    } else if (type == 1) {  // Dying - AR with negative drift
        mu = -0.003 + (rand() / (double)RAND_MAX) * 0.002;
        sigma = 0.005 + (rand() / (double)RAND_MAX) * 0.295;
    } else if (type == 2) {  // Volatile sideways - AR high vol
        mu = 0.0;
        sigma = 0.1 + (rand() / (double)RAND_MAX) * 0.3;
    } else if (type == 3) {  // Mean-Reverting - Geometric OU with AR
        double log_tmin = log(0.01);
        double log_tmax = log(100000.0);
        double log_target = log_tmin + (rand() / (double)RAND_MAX) * (log_tmax - log_tmin);
        target_mean = exp(log_target);
        theta = log(target_mean);
        kappa = 0.005 + (rand() / (double)RAND_MAX) * 0.045;
        sigma = 0.005 + (rand() / (double)RAND_MAX) * 0.295;
    } else if (type == 4) {  // Cyclical - AR with sinusoidal mu
        mu_base = -0.001 + (rand() / (double)RAND_MAX) * 0.002;
        amp_mu = 0.0005 + (rand() / (double)RAND_MAX) * 0.0045;
        period = 10 + rand() % 31;
        sigma = 0.005 + (rand() / (double)RAND_MAX) * 0.295;
    }

    // Write seed file
    fprintf(seed_out, "Trend type: %s\nVolatility (sigma): %.4f\nAR order: %d\n", trend_types[type], sigma, ar_order);
    fprintf(seed_out, "AR coefficients: phi1=%.4f, phi2=%.4f", phi[0], phi[1]);
    if (ar_order == 3) fprintf(seed_out, ", phi3=%.4f", phi[2]);
    fprintf(seed_out, "\n");
    if (type == 3) {
        fprintf(seed_out, "Target mean: %.2f\n", target_mean);
    } else if (type == 4) {
        fprintf(seed_out, "Mu base: %.4f\nMu amplitude: %.4f\nPeriod: %d\n", mu_base, amp_mu, period);
    }
    fclose(seed_out);

    // Initialize price history (log-prices)
    double log_prices[3] = {log(initial), log(initial), log(initial)};
    fprintf(out, "%.2f\n", initial);

    // Generate the remaining 49 prices
    for (int i = 1; i < 50; i++) {
        double log_new_val;
        double z = gaussian_random();
        if (type == 3) {  // Geometric OU
            log_new_val = log_prices[0] + kappa * (theta - log_prices[0]);
            log_new_val += phi[0] * (log_prices[0] - log_prices[1]);
            log_new_val += phi[1] * (log_prices[1] - log_prices[2]);
            if (ar_order == 3) log_new_val += phi[2] * (log_prices[2] - log_prices[0]);
            log_new_val += sigma * z;
        } else if (type == 4) {  // Cyclical
            double phase = 2.0 * M_PI * i / period;
            double mu_i = mu_base + amp_mu * sin(phase);
            log_new_val = mu_i + phi[0] * log_prices[0] + phi[1] * log_prices[1];
            if (ar_order == 3) log_new_val += phi[2] * log_prices[2];
            log_new_val += sigma * z;
        } else {  // Growing, Dying, Volatile Sideways
            log_new_val = mu + phi[0] * log_prices[0] + phi[1] * log_prices[1];
            if (ar_order == 3) log_new_val += phi[2] * log_prices[2];
            log_new_val += sigma * z;
        }

        double new_val = exp(log_new_val);
        if (new_val < 0.01) new_val = 0.01;  // Clip to positive
        fprintf(out, "%.2f\n", new_val);

        // Shift history
        log_prices[2] = log_prices[1];
        log_prices[1] = log_prices[0];
        log_prices[0] = log_new_val;
    }

    fclose(out);
    printf("Generated files: %s and %s\n", outname, seedname);
    return 0;
}
