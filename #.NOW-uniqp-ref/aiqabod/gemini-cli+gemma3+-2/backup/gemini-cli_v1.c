#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

// Struct to hold the response from the server
struct MemoryStruct {
  char *memory;
  size_t size;
};

// Callback function to write the response to our MemoryStruct
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <prompt>\n", argv[0]);
        return 1;
    }

    const char *prompt = argv[1];
    CURL *curl;
    CURLcode res;

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        char json_payload[1024];
        // WARNING: This is a simple and unsafe way to create a JSON string.
        // A robust implementation should use a JSON library to escape the prompt.
        snprintf(json_payload, sizeof(json_payload),
                 "{\"model\": \"gemma3:270M\", \"messages\": [{\"role\": \"user\", \"content\": \"%s\"}]}",
                 prompt);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/v1/chat/completions");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            cJSON *json = cJSON_Parse(chunk.memory);
            if (json == NULL) {
                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr != NULL) {
                    fprintf(stderr, "Error before: %s\n", error_ptr);
                }
            } else {
                const cJSON *choices = cJSON_GetObjectItemCaseSensitive(json, "choices");
                if (cJSON_IsArray(choices)) {
                    cJSON *choice = cJSON_GetArrayItem(choices, 0);
                    if (cJSON_IsObject(choice)) {
                        const cJSON *message = cJSON_GetObjectItemCaseSensitive(choice, "message");
                        if (cJSON_IsObject(message)) {
                            const cJSON *content = cJSON_GetObjectItemCaseSensitive(message, "content");
                            if (cJSON_IsString(content) && (content->valuestring != NULL)) {
                                printf("%s\n", content->valuestring);
                            }
                        }
                    }
                }
                cJSON_Delete(json);
            }
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(chunk.memory);
    }
    curl_global_cleanup();

    return 0;
}
