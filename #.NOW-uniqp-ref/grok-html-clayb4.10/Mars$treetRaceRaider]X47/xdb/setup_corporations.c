#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_CORPS 100
#define MAX_LINE_LEN 256

// Structure to hold corporation data
typedef struct {
    char name[100];
    char ticker[20];
    char industry[100];
} Corporation;

// Function to create a directory if it doesn't exist
void create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

// Function to trim trailing whitespace
void trim_trailing_whitespace(char *str) {
    int i = strlen(str) - 1;
    while (i >= 0 && isspace((unsigned char)str[i])) {
        str[i] = '\0';
        i--;
    }
}

// Function to read corporations from file
int read_corporations(Corporation corporations[]) {
   
    FILE *fp = fopen("corporations/martian_corporations.txt", "r");
    if (fp == NULL) {
        perror("Error opening martian_corporations.txt");
        return 0;
    }

    int count = 0;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) != NULL && count < MAX_CORPS) {
        line[strcspn(line, "\r\n")] = 0; // Remove newline chars

        // Find ticker and industry delimiters
        char *ticker_start = strrchr(line, '(');
        char *ticker_end = strrchr(line, ')');
        char *industry_start = strstr(line, "Ind:");
        if (ticker_start && ticker_end && industry_start && ticker_start < ticker_end && ticker_end < industry_start) {
            *ticker_start = '\0'; // Separate name
            *ticker_end = '\0';   // End ticker
            char *industry_info = industry_start + 4; // Skip "Ind:"
            while (isspace((unsigned char)*industry_info)) industry_info++; // Skip leading spaces

            // Trim trailing whitespace from name
            trim_trailing_whitespace(line);

            // Copy name, ticker, and industry to corporation struct
            strncpy(corporations[count].name, line, sizeof(corporations[count].name) - 1);
            corporations[count].name[sizeof(corporations[count].name) - 1] = '\0';

            strncpy(corporations[count].ticker, ticker_start + 1, sizeof(corporations[count].ticker) - 1);
            corporations[count].ticker[sizeof(corporations[count].ticker) - 1] = '\0';

            strncpy(corporations[count].industry, industry_info, sizeof(corporations[count].industry) - 1);
            corporations[count].industry[sizeof(corporations[count].industry) - 1] = '\0';

            count++;
        }
    }

    fclose(fp);
    return count;
}

// Function to generate a random float between min and max
float rand_float(float min, float max) {
    return min + (float)rand() / ((float)RAND_MAX / (max - min));
}

// Function to generate and save a corporation's financial sheet
void generate_corp_sheet(Corporation corp) {
    char file_path[256];
    sprintf(file_path, "corporations/generated/%s.txt", corp.ticker);

    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        perror("Error creating corporation sheet");
        return;
    }

    // Generate random financial data
    float free_cash = rand_float(100.0, 1000.0);
    float business_loans = rand_float(500.0, 2000.0);
    float consumer_loans = rand_float(100.0, 500.0);
    float mortgage_loans = rand_float(200.0, 1000.0);
    float bad_debt_reserves = rand_float(-300.0, -100.0);
    float total_assets = free_cash + business_loans + consumer_loans + mortgage_loans + bad_debt_reserves;
    float demand_deposits = rand_float(100.0, 500.0);
    float cd = rand_float(500.0, 1500.0);
    float total_liabilities = demand_deposits + cd;
    float net_worth = total_assets - total_liabilities;
    float shares_outstanding = rand_float(10.0, 100.0);
    float net_worth_per_share = net_worth / shares_outstanding;
    float stock_price = net_worth_per_share * rand_float(0.8, 1.5);
    float week_52_high = stock_price * rand_float(1.1, 2.0);
    float week_52_low = stock_price * rand_float(0.5, 0.9);
    float total_cap = stock_price * shares_outstanding;

    fprintf(fp, "FINANCIAL BALANCE SHEET: %s (%s):\n", corp.name, corp.ticker);
    fprintf(fp, "---------------------------------------------\n");
    fprintf(fp, "(Amounts in millions of U.S. Dollars, except per share amounts)\n");
    fprintf(fp, "ASSETS:\n");
    fprintf(fp, "Free Cash and Equivalents\t\t%.2f\n", free_cash);
    fprintf(fp, "Stock in Subsidiary Corps.\t\t0.00\n");
    fprintf(fp, "Options Long/Short: Net Value\t\t0.00\n");
    fprintf(fp, "Government Bonds (mkt. value)\t\t0.00\n");
    fprintf(fp, "Corporate Bonds (mkt. value)\t\t0.00\n");
    fprintf(fp, "Business Loan Portfolio\t\t%.2f\n", business_loans);
    fprintf(fp, "Consumer Loan Portfolio\t\t%.2f\n", consumer_loans);
    fprintf(fp, "Mortgage Loans/Securities\t\t%.2f\n", mortgage_loans);
    fprintf(fp, "Less: Bad Debt Reserves:\t\t%.2f\n", bad_debt_reserves);
    fprintf(fp, "----------------\n");
    fprintf(fp, "Total Assets:\t\t%.2f\n", total_assets);
    fprintf(fp, "----------------\n");
    fprintf(fp, "LIABILITIES AND EQUITY:\n");
    fprintf(fp, "Long-Term Bonds Issued\t\t0.00\n");
    fprintf(fp, "Demand Deposits\t\t%.2f\n", demand_deposits);
    fprintf(fp, "Certificates of Deposit\t\t%.2f\n", cd);
    fprintf(fp, "Interbank Debt -- Fed Funds\t\t0.00\n");
    fprintf(fp, "Accrued Income Tax Payable\t\t0.00\n");
    fprintf(fp, "----------------\n");
    fprintf(fp, "Total Liabilities:\t\t%.2f [1]\n", total_liabilities);
    fprintf(fp, "----------------\n");
    fprintf(fp, "Equity (Net Worth):\t\t%.2f\n", net_worth);
    fprintf(fp, "================\n");
    fprintf(fp, "STOCK AND EARNINGS DATA:\n");
    fprintf(fp, "--------------------------\n");
    fprintf(fp, "Net Worth Per Share:\t\t%.2f\n", net_worth_per_share);
    fprintf(fp, "Current Stock Price:\t\t%.2f\n", stock_price);
    fprintf(fp, "52-Week High/Low Stock Price:\t\t%.2f / %.2f\n", week_52_high, week_52_low);
    fprintf(fp, "Shares of stock outstanding:\t\t%.2f million\n", shares_outstanding);
    fprintf(fp, "Total stock capitalization:\t\t%.2f million\n", total_cap);
    fprintf(fp, "MISCELLANEOUS INFO:\n");
    fprintf(fp, "-------------------\n");
    fprintf(fp, "Industry Group:\t\t%s\n", corp.industry);

    fclose(fp);
}

int main() {
    srand(time(NULL));
 system("rm -r corporations/generated/*");
    create_directory("corporations/generated");

    Corporation corporations[MAX_CORPS] = {0};
    int num_corps = read_corporations(corporations);

    if (num_corps > 0) {
        for (int i = 0; i < num_corps; i++) {
            generate_corp_sheet(corporations[i]);
        }
        printf("%d corporation sheets generated successfully.\n", num_corps);
    } else {
        printf("No corporations found or error reading file.\n");
    }

    return 0;
}
