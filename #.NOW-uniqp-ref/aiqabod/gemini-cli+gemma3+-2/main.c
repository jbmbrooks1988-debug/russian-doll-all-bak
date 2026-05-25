#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <getopt.h>
#include <unistd.h> // For isatty

#include "file_utils.h"
#include "cli_utils.h"

// Global flag for auto-confirmation
int auto_confirm = 0;

// Struct to hold the stream buffer and full response
struct StreamBuffer {
    char *data; // For line buffering
    size_t size; // For line buffering
    char *full_response; // To accumulate full model response
    size_t full_response_size; // Size of full_response
};

// Processes a single line from the SSE stream
void process_stream_line(char* line, struct StreamBuffer *mem) {
    if (strncmp(line, "data: ", 6) == 0) {
        char* json_str = line + 6;
        if (strcmp(json_str, "[DONE]") == 0) {
            return;
        }

        cJSON *json = cJSON_Parse(json_str);
        if (json != NULL) {
            const cJSON *choices = cJSON_GetObjectItemCaseSensitive(json, "choices");
            if (cJSON_IsArray(choices)) {
                cJSON *choice = cJSON_GetArrayItem(choices, 0);
                if (cJSON_IsObject(choice)) {
                    const cJSON *delta = cJSON_GetObjectItemCaseSensitive(choice, "delta");
                    if (cJSON_IsObject(delta)) {
                        const cJSON *content = cJSON_GetObjectItemCaseSensitive(delta, "content");
                        if (cJSON_IsString(content) && content->valuestring != NULL) {
                            printf("%s", content->valuestring);
                            fflush(stdout);

                            // Accumulate full response
                            size_t content_len = strlen(content->valuestring);
                            // Remove trailing newline if present
                            if (content_len > 0 && content->valuestring[content_len - 1] == '\n') {
                                content_len--;
                            }
                            char *new_full_response = realloc(mem->full_response, mem->full_response_size + content_len + 1);
                            if (new_full_response == NULL) {
                                fprintf(stderr, "Failed to reallocate memory for full response.\n");
                            } else {
                                mem->full_response = new_full_response;
                                memcpy(&(mem->full_response[mem->full_response_size]), content->valuestring, content_len);
                                mem->full_response_size += content_len;
                                mem->full_response[mem->full_response_size] = 0;
                            }
                        }
                    }
                }
            }
            cJSON_Delete(json);
        }
    }
}


// Callback function to handle streaming data
static size_t
StreamCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct StreamBuffer *mem = (struct StreamBuffer *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if(ptr == NULL) {
        fprintf(stderr, "not enough memory (realloc returned NULL) for stream buffer\n");
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    char *line_end;
    while ((line_end = strchr(mem->data, '\n')) != NULL) {
        *line_end = '\0';
        if (strlen(mem->data) > 0 && mem->data[strlen(mem->data)-1] == '\r') {
            mem->data[strlen(mem->data)-1] = '\0';
        }
        process_stream_line(mem->data, mem);
        
        size_t line_len = strlen(mem->data) + 1;
        memmove(mem->data, mem->data + line_len, mem->size - line_len);
        mem->size -= line_len;
        mem->data[mem->size] = 0;
    }

    return realsize;
}

void print_usage(const char* prog_name) {
    fprintf(stderr, "Usage: %s [options] <prompt>\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --prompt <prompt>    The prompt to send to the model.\n");
    fprintf(stderr, "  --model <model_name> Override the model from config.\n");
    fprintf(stderr, "  --api-url <url>      Override the API URL from config.\n");
    fprintf(stderr, "  -y, --yes            Assume 'yes' to all confirmation prompts.\n");
    fprintf(stderr, "  --help               Show this help message.\n");
}

// Helper function to expand file references in a prompt
char* expand_file_references(const char* original_prompt) {
    char *expanded_prompt = strdup(""); // Start with an empty string
    if (expanded_prompt == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for expanded prompt.\n");
        return NULL;
    }

    const char *current_pos = original_prompt;
    const char *at_sign_pos;

    while ((at_sign_pos = strstr(current_pos, "@")) != NULL) {
        // Append text before the '@'
        size_t pre_at_len = at_sign_pos - current_pos;
        char *temp = realloc(expanded_prompt, strlen(expanded_prompt) + pre_at_len + 1);
        if (temp == NULL) {
            fprintf(stderr, "Error: Failed to reallocate memory for expanded prompt.\n");
            free(expanded_prompt);
            return NULL;
        }
        expanded_prompt = temp;
        strncat(expanded_prompt, current_pos, pre_at_len);
        expanded_prompt[strlen(expanded_prompt)] = '\0'; // Ensure null termination

        // Find end of file path (space, newline, or end of string)
        const char *path_start = at_sign_pos + 1;
        const char *path_end = path_start;
        while (*path_end != ' ' && *path_end != '\n' && *path_end != '\0') {
            path_end++;
        }

        if (path_end == path_start) { // Just an '@' without a path
            strcat(expanded_prompt, "@");
            current_pos = path_start;
            continue;
        }

        size_t path_len = path_end - path_start;
        char *file_path = (char*)malloc(path_len + 1);
        if (file_path == NULL) {
            fprintf(stderr, "Error: Failed to allocate memory for file path.\n");
            free(expanded_prompt);
            return NULL;
        }
        strncpy(file_path, path_start, path_len);
        file_path[path_len] = '\0';

        // Ask for confirmation before reading file
        char question[512];
        snprintf(question, sizeof(question), "Read content from file '%s'?", file_path);
        if (!ask_for_confirmation(question, auto_confirm)) {
            fprintf(stderr, "Skipping file '%s' due to user denial.\n", file_path);
            char *temp_append = realloc(expanded_prompt, strlen(expanded_prompt) + 1 + strlen(file_path) + 1); // +1 for '@', +1 for null terminator
            if (temp_append == NULL) {
                fprintf(stderr, "Error: Failed to reallocate memory for expanded prompt.\n");
                free(expanded_prompt);
                free(file_path);
                return NULL;
            }
            expanded_prompt = temp_append;
            strcat(expanded_prompt, "@");
            strcat(expanded_prompt, file_path);
            free(file_path);
            current_pos = path_end;
            continue; // Skip to next iteration
        }

        char* file_content = read_file_to_string(file_path);
        if (file_content == NULL) {
            fprintf(stderr, "Error: Failed to read content from file '%s'.\n", file_path);
            free(expanded_prompt);
            free(file_path);
            return NULL;
        }
        free(file_path); // Free file_path after use

        char *temp_content = realloc(expanded_prompt, strlen(expanded_prompt) + strlen(file_content) + 1);
        if (temp_content == NULL) {
            fprintf(stderr, "Error: Failed to reallocate memory for expanded prompt content.\n");
            free(file_content);
            free(expanded_prompt);
            return NULL;
        }
        expanded_prompt = temp_content;
        strcat(expanded_prompt, file_content);
        free(file_content); // Free file_content after use

        current_pos = path_end;
    }

    // Append any remaining text after the last '@'
    if (*current_pos != '\0') {
        char *temp = realloc(expanded_prompt, strlen(expanded_prompt) + strlen(current_pos) + 1);
        if (temp == NULL) {
            fprintf(stderr, "Error: Failed to reallocate memory for expanded prompt.\n");
            free(expanded_prompt);
            return NULL;
        }
        expanded_prompt = temp;
        strcat(expanded_prompt, current_pos);
    }

    return expanded_prompt;
}


int main(int argc, char *argv[]) {
    fprintf(stderr, "DEBUG: Main function started.\n"); // Added debug print
    // --- Argument Parsing ---
    char *prompt_arg = NULL;
    char *model_arg = NULL;
    char *api_url_arg = NULL;

    struct option long_options[] = {
        {"prompt",  required_argument, 0, 'p'},
        {"model",   required_argument, 0, 'm'},
        {"api-url", required_argument, 0, 'u'},
        {"yes",     no_argument,       0, 'y'}, // New option
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "p:m:u:yh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'p': prompt_arg = strdup(optarg); break;
            case 'm': model_arg = strdup(optarg); break;
            case 'u': api_url_arg = strdup(optarg); break;
            case 'y': auto_confirm = 1; break; // Set global flag
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (optind < argc) {
        if (prompt_arg != NULL) {
            fprintf(stderr, "Error: Prompt provided with --prompt and as a bare argument. Please use only one.\n");
            print_usage(argv[0]);
            free(prompt_arg);
            if (model_arg) free(model_arg);
            if (api_url_arg) free(api_url_arg);
            return 1;
        }
        prompt_arg = strdup(argv[optind]);
    }

    if (prompt_arg == NULL) {
        // Check if stdin is a terminal or a pipe/file
        if (isatty(STDIN_FILENO)) {
            // Interactive mode
            fprintf(stderr, "Enter your prompt (Ctrl+D to finish):\n");
            char *line = NULL;
            size_t len = 0;
            ssize_t read;
            char *multiline_prompt = NULL;
            size_t total_len = 0;

            while ((read = getline(&line, &len, stdin)) != -1) {
                // Remove trailing newline from each line
                if (read > 0 && line[read - 1] == '\n') {
                    line[read - 1] = '\0';
                    read--; // Adjust read length
                }

                // Add a space between lines if not the very first line and something was read
                if (total_len > 0 && read > 0) {
                    char *space_prompt = realloc(multiline_prompt, total_len + 1 + read + 1); // +1 for space, +read for new line, +1 for null terminator
                    if (space_prompt == NULL) {
                        fprintf(stderr, "Error: Failed to allocate memory for multiline prompt.\n");
                        free(line);
                        free(multiline_prompt);
                        if (model_arg) free(model_arg);
                        if (api_url_arg) free(api_url_arg);
                        return 1;
                    }
                    multiline_prompt = space_prompt;
                    multiline_prompt[total_len] = ' ';
                    total_len++;
                    multiline_prompt[total_len] = '\0';
                }

                char *new_multiline_prompt = realloc(multiline_prompt, total_len + read + 1);
                if (new_multiline_prompt == NULL) {
                    fprintf(stderr, "Error: Failed to allocate memory for multiline prompt.\n");
                    free(line);
                    free(multiline_prompt);
                    if (model_arg) free(model_arg);
                    if (api_url_arg) free(api_url_arg);
                    return 1;
                }
                multiline_prompt = new_multiline_prompt;
                memcpy(multiline_prompt + total_len, line, read);
                total_len += read;
                multiline_prompt[total_len] = '\0';
            }
            free(line); // Free the buffer used by getline

            if (multiline_prompt == NULL || total_len == 0) {
                fprintf(stderr, "Error: No prompt entered.\n");
                if (model_arg) free(model_arg);
                if (api_url_arg) free(api_url_arg);
                return 1;
            }
            prompt_arg = multiline_prompt;
        } else {
            // Piped input mode
            char *piped_input = NULL;
            size_t piped_len = 0;
            char buffer[4096];
            size_t bytes_read;

            while ((bytes_read = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
                char *new_piped_input = realloc(piped_input, piped_len + bytes_read + 1);
                if (new_piped_input == NULL) {
                    fprintf(stderr, "Error: Failed to allocate memory for piped input.\n");
                    free(piped_input);
                    if (model_arg) free(model_arg);
                    if (api_url_arg) free(api_url_arg);
                    return 1;
                }
                piped_input = new_piped_input;
                memcpy(piped_input + piped_len, buffer, bytes_read);
                piped_len += bytes_read;
                piped_input[piped_len] = '\0';
            }

            if (piped_input == NULL || piped_len == 0) {
                fprintf(stderr, "Error: No piped input received.\n");
                if (model_arg) free(model_arg);
                if (api_url_arg) free(api_url_arg);
                return 1;
            }
            prompt_arg = piped_input;
        }
    }

    // --- Configuration Loading & Precedence ---
    const char *default_model = "gemma3:270M";
    const char *default_api_url = "http://localhost:11434/v1/chat/completions";
    char *model = NULL;
    char *api_url = NULL;

    if (model_arg) model = model_arg;
    if (api_url_arg) api_url = api_url_arg;

    if (model == NULL || api_url == NULL) {
        char* config_buffer = read_file_to_string("config.json");
        if (config_buffer != NULL) {
            cJSON *config_json = cJSON_Parse(config_buffer);
            free(config_buffer);
            if (config_json != NULL) {
                if (model == NULL) {
                    const cJSON *model_json = cJSON_GetObjectItemCaseSensitive(config_json, "model");
                    if (cJSON_IsString(model_json) && model_json->valuestring != NULL) {
                        model = strdup(model_json->valuestring);
                    }
                }
                if (api_url == NULL) {
                    const cJSON *api_url_json = cJSON_GetObjectItemCaseSensitive(config_json, "api_url");
                    if (cJSON_IsString(api_url_json) && api_url_json->valuestring != NULL) {
                        api_url = strdup(api_url_json->valuestring);
                    }
                }
                cJSON_Delete(config_json);
            }
        }
    }

    if (model == NULL) model = (char*)default_model;
    if (api_url == NULL) api_url = (char*)default_api_url;

    // --- File Reference Expansion ---
    char *original_prompt_to_free = prompt_arg; // Keep track of original prompt_arg for freeing
    char *expanded_prompt = expand_file_references(prompt_arg);
    if (expanded_prompt == NULL) {
        fprintf(stderr, "Error: Failed to expand file references in prompt.\n");
        // Cleanup resources before returning
        free(original_prompt_to_free); // Free the original prompt_arg
        if (model != default_model) free(model);
        if (api_url != default_api_url) free(api_url);
        return 1;
    }
    prompt_arg = expanded_prompt; // Use the expanded prompt

    // --- Conversation History Loading ---
    cJSON *history_array = NULL;
    char* history_buffer = read_file_to_string("history.json");
    if (history_buffer != NULL) {
        history_array = cJSON_Parse(history_buffer);
        free(history_buffer);
        if (history_array == NULL || !cJSON_IsArray(history_array)) {
            fprintf(stderr, "Warning: history.json is invalid or not an array. Starting new history.\n");
            if (history_array) cJSON_Delete(history_array);
            history_array = cJSON_CreateArray();
        }
    } else {
        history_array = cJSON_CreateArray();
    }
    if (history_array == NULL) {
        fprintf(stderr, "Error: Failed to initialize conversation history.\n");
        free(prompt_arg); // Free the expanded prompt
        if (model != default_model) free(model);
        if (api_url != default_api_url) free(api_url);
        return 1;
    }

    // Add current user prompt to history
    cJSON *user_message = cJSON_CreateObject();
    cJSON_AddStringToObject(user_message, "role", "user");
    cJSON_AddStringToObject(user_message, "content", prompt_arg);
    cJSON_AddItemToArray(history_array, user_message);


    // --- Main Logic ---
    CURL *curl;
    CURLcode res;
    struct StreamBuffer chunk = { .data = malloc(1), .size = 0, .full_response = malloc(1), .full_response_size = 0 };
    if (chunk.data == NULL || chunk.full_response == NULL) {
        fprintf(stderr, "Error: Failed to allocate initial memory for stream buffer or full response.\n");
        cJSON_Delete(history_array);
        free(prompt_arg); // Free the expanded prompt
        if (model != default_model) free(model);
        if (api_url != default_api_url) free(api_url);
        return 1;
    }
    chunk.data[0] = 0;
    chunk.full_response[0] = 0;


    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        // --- TEMPORARY DEMO: Simulate LLM response with write_file tool call ---
        const char* simulated_response = "{\"tool_code\": \"write_file\", \"file_path\": \"./test_output.txt\", \"content\": \"Hello from gemini-cli write_file tool!\\nThis is a test of the write_file functionality.\\n\"}";
        chunk.full_response = realloc(chunk.full_response, strlen(simulated_response) + 1);
        if (chunk.full_response == NULL) {
            fprintf(stderr, "Error: Failed to reallocate memory for simulated response.\n");
            // Cleanup and exit
            cJSON_Delete(history_array);
            free(prompt_arg);
            if (model != default_model) free(model);
            if (api_url != default_api_url) free(api_url);
            free(chunk.data);
            free(chunk.full_response);
            curl_global_cleanup();
            return 1;
        }
        strcpy(chunk.full_response, simulated_response);
        chunk.full_response_size = strlen(simulated_response);
        res = CURLE_OK; // Simulate a successful curl operation
        // --- END TEMPORARY DEMO ---
        fprintf(stderr, "DEBUG: chunk.full_response set to: %s\n", chunk.full_response); // Added debug print

        // Original curl logic (commented out for demo)
        /*
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "model", model);
        cJSON_AddBoolToObject(root, "stream", 1);
        
        // Add cloned history to the request
        cJSON *messages_for_request = cJSON_Duplicate(history_array, 1); // Deep copy
        cJSON_AddItemToObject(root, "messages", messages_for_request);

        char *json_payload = cJSON_PrintUnformatted(root);
        cJSON_Delete(root); // Delete root and its children (including messages_for_request)

        if (json_payload != NULL) {
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_URL, api_url);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

            res = curl_easy_perform(curl);
            free(json_payload);
            curl_slist_free_all(headers);

            if(res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                printf("\n"); // Newline after streamed response

                // Check for and execute tool calls
                fprintf(stderr, "DEBUG: Attempting to parse full_response: %s\n", chunk.full_response);
                cJSON *response_json = cJSON_Parse(chunk.full_response);
                if (response_json != NULL) {
                    fprintf(stderr, "DEBUG: JSON parsed successfully.\n");
                    cJSON *tool_code = cJSON_GetObjectItemCaseSensitive(response_json, "tool_code");
                    fprintf(stderr, "DEBUG: tool_code cJSON object: %p\n", (void*)tool_code);
                    if (cJSON_IsString(tool_code) && strcmp(tool_code->valuestring, "write_file") == 0) {
                        fprintf(stderr, "DEBUG: tool_code is 'write_file'.\n");
                        cJSON *file_path_json = cJSON_GetObjectItemCaseSensitive(response_json, "file_path");
                        cJSON *content_json = cJSON_GetObjectItemCaseSensitive(response_json, "content");
                        fprintf(stderr, "DEBUG: file_path_json cJSON object: %p, content_json cJSON object: %p\n", (void*)file_path_json, (void*)content_json);

                        if (cJSON_IsString(file_path_json) && cJSON_IsString(content_json)) {
                            fprintf(stderr, "DEBUG: file_path: '%s', content: '%s'\n", file_path_json->valuestring, content_json->valuestring);
                            fprintf(stderr, "Executing tool: write_file to '%s'\n", file_path_json->valuestring);
                            if (write_string_to_file(file_path_json->valuestring, content_json->valuestring, auto_confirm) == 0) {
                                fprintf(stderr, "Successfully wrote to '%s'.\n", file_path_json->valuestring);
                            } else {
                                fprintf(stderr, "Failed to write to '%s'.\n", file_path_json->valuestring);
                            }
                        } else {
                            fprintf(stderr, "Warning: Malformed write_file tool call. Missing file_path or content.\n");
                        }
                    } else {
                        fprintf(stderr, "DEBUG: Not a 'write_file' tool call or tool_code is not a string.\n");
                        // If it's not a tool call, or a tool call we don't handle,
                        // treat the full response as assistant content.
                        // Add model's full response to history
                        if (chunk.full_response_size > 0) {
                            cJSON *assistant_message = cJSON_CreateObject();
                            cJSON_AddStringToObject(assistant_message, "role", "assistant");
                            cJSON_AddStringToObject(assistant_message, "content", chunk.full_response);
                            cJSON_AddItemToArray(history_array, assistant_message);
                        }
                    }
                    cJSON_Delete(response_json);
                } else {
                    fprintf(stderr, "DEBUG: JSON parsing failed for full_response.\n");
                    // If the response is not valid JSON, treat it as plain text content
                    if (chunk.full_response_size > 0) {
                        cJSON *assistant_message = cJSON_CreateObject();
                        cJSON_AddStringToObject(assistant_message, "role", "assistant");
                        cJSON_AddStringToObject(assistant_message, "content", chunk.full_response);
                        cJSON_AddItemToArray(history_array, assistant_message);
                    }
                }

                // Save updated history to file
                char *history_json_str = cJSON_PrintUnformatted(history_array);
                if (history_json_str != NULL) {
                    FILE *fp = fopen("history.json", "wb");
                    if (fp != NULL) {
                        fputs(history_json_str, fp);
                        fclose(fp);
                    } else {
                        fprintf(stderr, "Warning: Failed to save history.json\n");
                    }
                    free(history_json_str);
                }
            }
        }
        curl_easy_cleanup(curl);
    }
    free(chunk.data);
    free(chunk.full_response);

    // --- Cleanup ---
    cJSON_Delete(history_array); // Free the history array and its contents
    free(original_prompt_to_free); // Free the original prompt_arg
    if (model != default_model) free(model);
    if (api_url != default_api_url) free(api_url);

    curl_global_cleanup();
    return 0;
}
