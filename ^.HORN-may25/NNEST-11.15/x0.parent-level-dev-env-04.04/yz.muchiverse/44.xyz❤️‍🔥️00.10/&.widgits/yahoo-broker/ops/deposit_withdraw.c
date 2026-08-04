/* deposit_withdraw - transfer funds between bank and broker account.
 * Reads/writes both bank config.txt and broker_state.txt.
 *
 * Usage: deposit_withdraw.+x <bank_session_dir> <direction> <amount>
 *   direction: deposit | withdraw
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            snprintf(out, out_sz, "%s", line + key_len + 1);
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *val) {
    char tmp[MAX_PATH + 64];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    FILE *orig = fopen(path, "r");
    if (orig) {
        char line[MAX_LINE];
        int found = 0;
        while (fgets(line, sizeof(line), orig)) {
            if (strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=') {
                fprintf(f, "%s=%s\n", key, val);
                found = 1;
            } else {
                fputs(line, f);
            }
        }
        if (!found) fprintf(f, "%s=%s\n", key, val);
        fclose(orig);
    } else {
        fprintf(f, "%s=%s\n", key, val);
    }
    fclose(f);
    rename(tmp, path);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <bank_session> <deposit|withdraw> <amount>\n", argv[0]);
        return 1;
    }

    const char *bank_session = argv[1];
    const char *direction = argv[2];
    const char *amount_str = argv[3];

    char bank_config[MAX_PATH];
    snprintf(bank_config, sizeof(bank_config), "%s/pieces/system/config.txt", bank_session);
    char broker_state[MAX_PATH];
    snprintf(broker_state, sizeof(broker_state), "%s/pieces/system/broker_state.txt", bank_session);

    char bank_balance_str[64] = "0.00";
    read_kv_str(bank_config, "bank_balance", bank_balance_str, sizeof(bank_balance_str));
    float bank_balance = atof(bank_balance_str);

    char broker_balance_str[64] = "0.00";
    read_kv_str(broker_state, "broker_balance", broker_balance_str, sizeof(broker_balance_str));
    float broker_balance = atof(broker_balance_str);

    float amount = atof(amount_str);
    if (amount <= 0) amount = 0;

    char buf[64];
    if (strcmp(direction, "deposit") == 0) {
        if (amount > bank_balance) amount = bank_balance;
        bank_balance -= amount;
        broker_balance += amount;
        snprintf(buf, sizeof(buf), "%.2f", bank_balance);
        write_kv(bank_config, "bank_balance", buf);
        snprintf(buf, sizeof(buf), "%.2f", broker_balance);
        write_kv(broker_state, "broker_balance", buf);
        printf("Deposited $%.2f. Broker balance: $%.2f\n", amount, broker_balance);
    } else if (strcmp(direction, "withdraw") == 0) {
        if (amount > broker_balance) amount = broker_balance;
        bank_balance += amount;
        broker_balance -= amount;
        snprintf(buf, sizeof(buf), "%.2f", bank_balance);
        write_kv(bank_config, "bank_balance", buf);
        snprintf(buf, sizeof(buf), "%.2f", broker_balance);
        write_kv(broker_state, "broker_balance", buf);
        printf("Withdrew $%.2f. Bank balance: $%.2f\n", amount, bank_balance);
    } else {
        fprintf(stderr, "Unknown direction: %s\n", direction);
        return 1;
    }

    return 0;
}
