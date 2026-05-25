#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

#define MAX_INDUSTRIES 50
#define MAX_LEN 100

void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

char* get_active_entity() {
    FILE *fp = fopen("data/active_entity.txt", "r");
    if (fp == NULL) return "N/A";
    static char entity[100];
    fgets(entity, sizeof(entity), fp);
    fclose(fp);
    char *newline = strchr(entity, '\n');
    if (newline) *newline = '\0';
    return entity;
}

int is_corp_name_taken(const char* name) {
    DIR *d;
    struct dirent *dir;
    d = opendir("corporations/generated");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) {
                char filepath[256];
                sprintf(filepath, "corporations/generated/%s", dir->d_name);
                FILE *fp = fopen(filepath, "r");
                if (fp) {
                    char line[256];
                    if (fgets(line, sizeof(line), fp)) {
                        char *start = strstr(line, "FINANCIAL PROFILE FOR ");
                        if (start) {
                            start += strlen("FINANCIAL PROFILE FOR ");
                            char *end = strstr(start, " (");
                            if (end) {
                                *end = '\0';
                                if (strcmp(start, name) == 0) {
                                    fclose(fp);
                                    closedir(d);
                                    return 1; // Name is taken
                                }
                            }
                        }
                    }
                    fclose(fp);
                }
            }
        }
        closedir(d);
    }
    return 0; // Name is not taken
}

char* str_replace(char *orig, const char *rep, const char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig || !rep) return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0) return NULL;
    if (!with) with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result) return NULL;

    char *p = tmp;
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        p = strncpy(p, orig, len_front) + len_front;
        p = strcpy(p, with) + len_with;
        orig += len_front + len_rep;
    }
    strcpy(p, orig);
    return result;
}

void create_corporation_file(const char* name, const char* ticker, int industry_choice, const char* industry_name, int funding_amount, const char* country_name) {
    char template_path[256];
    if (industry_choice == 1) { // Bank
        strcpy(template_path, "corporations/bank_example.txt");
    } else if (industry_choice == 2) { // Insurance
        strcpy(template_path, "corporations/insurance_example.txt");
    } else {
        strcpy(template_path, "corporations/corporation_example.txt");
    }

    FILE *template_fp = fopen(template_path, "r");
    if (template_fp == NULL) {
        printf("Error: Could not open template file.\n");
        return;
    }

    char new_corp_path[256];
    sprintf(new_corp_path, "corporations/generated/%s.txt", ticker);
    FILE *new_corp_fp = fopen(new_corp_path, "w");
    if (new_corp_fp == NULL) {
        printf("Error: Could not create corporation file.\n");
        fclose(template_fp);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), template_fp)) {
        char* replaced_line = str_replace(line, "[CORPORATION NAME]", name);
        char* tmp = replaced_line;
        replaced_line = str_replace(tmp, "[TICKER]", ticker);
        free(tmp);
        tmp = replaced_line;
        replaced_line = str_replace(tmp, "[COUNTRY]", country_name);
        free(tmp);

        if (strstr(replaced_line, "Total Stock Capitalization:") || strstr(replaced_line, "Total Assets:") || strstr(replaced_line, "  Total Deposits:")) {
            char* key = strtok(replaced_line, ":");
            fprintf(new_corp_fp, "%s: %d.00\n", key, funding_amount);
        } else if (strstr(replaced_line, ":")) {
            char* key = strtok(replaced_line, ":");
            fprintf(new_corp_fp, "%s: 0.00\n", key);
        } else {
            fputs(replaced_line, new_corp_fp);
        }
        free(replaced_line);
    }

    fclose(template_fp);
    fclose(new_corp_fp);

    FILE *martian_fp = fopen("corporations/martian_corporations.txt", "a");
    if (martian_fp) {
        fprintf(martian_fp, "%s (%s) Ind: %s\n", name, ticker, industry_name);
        fclose(martian_fp);
    }

    printf("\nCongratulations! You have successfully founded %s (%s).\n", name, ticker);
    printf("Press Enter to continue...");
    getchar();
}


void startup_new_corp() {
    clear_screen();
    printf("--- Startup New Corp ---\n");

    FILE *fp = fopen("corporations/36_industries_wsr.txt", "r");
    if (fp == NULL) {
        printf("Error: Could not open industries file.\n");
        return;
    }

    char industries[MAX_INDUSTRIES][MAX_LEN];
    int num_industries = 0;
    while (fgets(industries[num_industries], MAX_LEN, fp) && num_industries < MAX_INDUSTRIES) {
        industries[num_industries][strcspn(industries[num_industries], "\n")] = 0;
        num_industries++;
    }
    fclose(fp);

    int industry_choice;
    while (1) {
        printf("Select the industry for your new corporation:\n");
        for (int i = 0; i < num_industries; i++) {
            printf("%s\n", industries[i]);
        }
        printf("\nEnter your choice (1-%d): ", num_industries);
        fflush(stdout);
        if (scanf("%d", &industry_choice) == 1 && industry_choice >= 1 && industry_choice <= num_industries) {
            while (getchar() != '\n');
            break;
        } else {
            printf("Invalid choice. Please try again.\n");
            while (getchar() != '\n');
        }
    }
    char* chosen_industry_str = industries[industry_choice - 1];
    char* industry_name = strchr(chosen_industry_str, '.');
    if (industry_name != NULL) {
        industry_name++;
        while (*industry_name == ' ') industry_name++;
    } else {
        industry_name = chosen_industry_str;
    }

    fp = fopen("governments/gov-list.txt", "r");
    if (fp == NULL) {
        printf("Error: Could not open governments file.\n");
        return;
    }
    char governments[MAX_INDUSTRIES][MAX_LEN];
    int num_governments = 0;
    char header[MAX_LEN];
    fgets(header, MAX_LEN, fp);
    while (fgets(governments[num_governments], MAX_LEN, fp) && num_governments < MAX_INDUSTRIES) {
        governments[num_governments][strcspn(governments[num_governments], "\n")] = 0;
        num_governments++;
    }
    fclose(fp);
    int country_choice;
    while (1) {
        printf("\nSelect the country of incorporation:\n");
        for (int i = 0; i < num_governments; i++) {
            char temp[MAX_LEN];
            strcpy(temp, governments[i]);
            char* c_name = strtok(temp, "\t");
            c_name = strtok(NULL, "\t");
            printf("%d. %s\n", i + 1, c_name);
        }
        printf("\nEnter your choice (1-%d): ", num_governments);
        fflush(stdout);
        if (scanf("%d", &country_choice) == 1 && country_choice >= 1 && country_choice <= num_governments) {
            while (getchar() != '\n');
            break;
        } else {
            printf("Invalid choice. Please try again.\n");
            while (getchar() != '\n');
        }
    }
    char chosen_country_line[MAX_LEN];
    strcpy(chosen_country_line, governments[country_choice - 1]);
    char* country_name = strtok(chosen_country_line, "\t");
    country_name = strtok(NULL, "\t");

    int funding_amount;
    int min_funding = 100;
    if (industry_choice == 1 || industry_choice == 2) {
        min_funding = 1000;
    }

    while (1) {
        printf("Enter the amount to fund the corporation (minimum %d): ", min_funding);
        fflush(stdout);
        if (scanf("%d", &funding_amount) == 1 && funding_amount >= min_funding) {
            while (getchar() != '\n');
            break;
        } else {
            printf("Invalid amount. Please enter a number greater than or equal to %d.\n", min_funding);
            while (getchar() != '\n');
        }
    }

    char corp_name[100];
    while (1) {
        printf("Enter the name for the new corporation (5-25 characters): ");
        fflush(stdout);
        fgets(corp_name, sizeof(corp_name), stdin);
        corp_name[strcspn(corp_name, "\n")] = 0;

        if (strlen(corp_name) >= 5 && strlen(corp_name) <= 25) {
            if (!is_corp_name_taken(corp_name)) {
                break;
            } else {
                printf("Corporation name already exists. Please choose another one.\n");
            }
        } else {
            printf("Invalid name length. Please try again.\n");
        }
    }

    char ticker[10];
    char upper_ticker[10];
    while (1) {
        printf("Enter the ticker symbol (1-6 characters): ");
        fflush(stdout);
        fgets(ticker, sizeof(ticker), stdin);
        ticker[strcspn(ticker, "\n")] = 0;

        for (int i = 0; ticker[i]; i++) {
            upper_ticker[i] = toupper(ticker[i]);
        }
        upper_ticker[strlen(ticker)] = '\0';

        if (strlen(upper_ticker) >= 1 && strlen(upper_ticker) <= 6) {
            char filepath[256];
            sprintf(filepath, "corporations/generated/%s.txt", upper_ticker);
            FILE *test_fp = fopen(filepath, "r");
            if (test_fp == NULL) {
                break;
            } else {
                fclose(test_fp);
                printf("Ticker symbol already exists. Please choose another one.\n");
            }
        } else {
            printf("Invalid ticker length. Please try again.\n");
        }
    }

    create_corporation_file(corp_name, upper_ticker, industry_choice, industry_name, funding_amount, country_name);
}

void display_financing_menu(const char* player_name) {
    char* active_entity = get_active_entity();
    int is_player_active = (strcmp(active_entity, player_name) == 0);
    int choice;
    int back_option = is_player_active ? 3 : 9;

    do {
        clear_screen();
        printf("--- Financing ---\n");

        if (is_player_active) {
            printf("1. Startup New Corp\n");
            printf("2. Capital Contribution\n");
            printf("3. Back to main menu\n");
        } else {
            printf("1. Startup New Corp\n");
            printf("2. Capital Contribution\n");
            printf("3. Public Stock Offering\n");
            printf("4. Issue Bonds\n");
            printf("5. Buy Back or Call Bonds\n");
            printf("6. Extraordinary Dividend\n");
            printf("7. Tax-Free Liquidation\n");
            printf("8. Spin Off Subsidiary\n");
            printf("9. Back to main menu\n");
        }

        printf("\nEnter your choice: ");
        fflush(stdout);
        scanf("%d", &choice);
        while (getchar() != '\n');

        if (choice == back_option) {
            break;
        }

        switch (choice) {
            case 1:
                startup_new_corp();
                break;
            default:
                printf("\nSelection %d is not yet implemented.", choice);
                printf("\nPress Enter to continue...");
                getchar();
                break;
        }

    } while (choice != back_option);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./financing player_name\n");
        return 1;
    }
    display_financing_menu(argv[1]);
    return 0;
}
