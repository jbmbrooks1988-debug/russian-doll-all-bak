#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
//🥈️
#define MAX_LINES 1000
#define M_PI 3.14159265358979323846

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s input.txt\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("fopen input");
        return 1;
    }

    double data[MAX_LINES];
    int n = 0;
    char line[100];
    while (fgets(line, sizeof(line), fp)) {
        data[n++] = atof(line);
        if (n >= MAX_LINES) {
            printf("Too many lines\n");
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);

    if (n < 3) {  // Need at least 3 points for std_res with df=1
        printf("Too few data points\n");
        return 1;
    }

    // Compute sums for OLS: y ~ const + x
    double sumx = 0, sumy = 0, sumxy = 0, sumx2 = 0;
    int m = n - 1;
    for (int i = 0; i < m; i++) {
        double x = data[i];
        double y = data[i + 1];
        sumx += x;
        sumy += y;
        sumxy += x * y;
        sumx2 += x * x;
    }

    double denom = m * sumx2 - sumx * sumx;
    if (fabs(denom) < 1e-9) {
        printf("Singular matrix\n");
        return 1;
    }

    double b = (m * sumxy - sumx * sumy) / denom;
    double a = (sumy - b * sumx) / m;

    // Compute residual sum of squares
    double sum_res2 = 0;
    for (int i = 0; i < m; i++) {
        double pred = a + b * data[i];
        double res = data[i + 1] - pred;
        sum_res2 += res * res;
    }
    double var_res = sum_res2 / (m - 2);  // Unbiased estimator
    double std_res = sqrt(var_res);

    // Generate output filename (e.g., TSLA.txt -> TSLA_GEN.txt)
    char outname[256];
    char *dot = strrchr(argv[1], '.');
    if (dot) {
        strncpy(outname, argv[1], dot - argv[1]);
        outname[dot - argv[1]] = '\0';
        strcat(outname, "_GEN.txt");
    } else {
        strcpy(outname, argv[1]);
        strcat(outname, "_GEN.txt");
    }

    FILE *out = fopen(outname, "w");
    if (!out) {
        perror("fopen output");
        return 1;
    }

    // Seed random and generate 50 new values
    srand(time(NULL));
    double last = data[n - 1];
    for (int i = 0; i < 50; i++) {
        // Box-Muller for standard normal
        double u1 = (double)rand() / RAND_MAX;
        double u2 = (double)rand() / RAND_MAX;
        double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);

        double noise = z * std_res;
        double new_val = a + b * last + noise;
        fprintf(out, "%.1f\n", new_val);
        last = new_val;
    }

    fclose(out);
    return 0;
}
