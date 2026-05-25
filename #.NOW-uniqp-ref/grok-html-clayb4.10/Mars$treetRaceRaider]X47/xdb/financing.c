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

char* str_replace(char *orig, char *rep, char *with) {
    char *result;
    char *ins;
    char *tmp;
    int len_rep;
    int len_with;
    int len_front;
    int count;

    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL;
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep;
    }
    strcpy(tmp, orig);
    return result;
}

void create_corporation_file(const char* name, const char* ticker, int industry_choice, int funding_amount) {
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

    fseek(template_fp, 0, SEEK_END);
    long fsize = ftell(template_fp);
    fseek(template_fp, 0, SEEK_SET);
    char *template_content = malloc(fsize + 1);
    fread(template_content, 1, fsize, template_fp);
    fclose(template_fp);
    template_content[fsize] = 0;

    char funding_str[20];
    sprintf(funding_str, "%d", funding_amount);
    char* content_with_name = str_replace(template_content, "[CORPORATION NAME]", (char*)name);
    char* content_with_ticker = str_replace(content_with_name, "[TICKER]", (char*)ticker);
    char* final_content = str_replace(content_with_ticker, "[FUNDING AMOUNT]", funding_str);

    char new_corp_path[256];
    sprintf(new_corp_path, "corporations/generated/%s.txt", ticker);
    FILE *new_corp_fp = fopen(new_corp_path, "w");
    if (new_corp_fp == NULL) {
        printf("Error: Could not create corporation file.\n");
    } else {
        fputs(final_content, new_corp_fp);
        fclose(new_corp_fp);
    }

    free(template_content);
    free(content_with_name);
    free(content_with_ticker);
    free(final_content);

    FILE *martian_fp = fopen("corporations/martian_corporations.txt", "a");
    if (martian_fp) {
        fprintf(martian_fp, "%s\n", ticker);
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
    while (1) {
        printf("Enter the ticker symbol (1-6 characters): ");
        fflush(stdout);
        fgets(ticker, sizeof(ticker), stdin);
        ticker[strcspn(ticker, "\n")] = 0;

        if (strlen(ticker) >= 1 && strlen(ticker) <= 6) {
            char filepath[256];
            sprintf(filepath, "corporations/generated/%s.txt", ticker);
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

    create_corporation_file(corp_name, ticker, industry_choice, funding_amount);
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
