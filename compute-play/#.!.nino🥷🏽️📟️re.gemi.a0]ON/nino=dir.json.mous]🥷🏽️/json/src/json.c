#include "json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_ERROR_SIZE 64
#define JSON_STRING_SIZE 16
#define JSON_ARRAY_SIZE 16

typedef enum JsonTokenType {
    TOKEN_EMPTY = 0,
    TOKEN_ERROR,
    TOKEN_NULL,
    TOKEN_BOOLEAN,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_LBRACE = '{',
    TOKEN_RBRACE = '}',
    TOKEN_LBRACKET = '[',
    TOKEN_RBRACKET = ']',
    TOKEN_COMMA = ',',
    TOKEN_COLON = ':',
} JsonTokenType;

typedef struct JsonToken {
    JsonTokenType type;
    union {
        double number;
        bool boolean;
        char* string;
    };
} JsonToken;

static JsonToken tokenError(Arena* arena, const char* fmt, ...) {
    JsonToken token;
    token.type = TOKEN_ERROR;
    token.string = arenaAlloc(arena, JSON_ERROR_SIZE);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(token.string, JSON_ERROR_SIZE, fmt, ap);
    va_end(ap);

    return token;
}

static JsonToken nextToken(const char* text, Arena* arena) {
    static const char* start = NULL;
    static const char* p = NULL;
    JsonToken token = {0};
    char c;

    if (text) {
        start = p = text;
    }

    if (!p) {
        return token;
    }

    while ((c = *p++)) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        } else if ((c >= '0' && c <= '9') || c == '-') {
            token.type = TOKEN_NUMBER;
            double sign = 1;
            double value = 0.0f;
            double exp_sign = 10;
            double exp = 0.0f;
            if (c == '-') {
                sign = -1;
                c = *p++;
                if (!(c >= '0' && c <= '9')) {
                    return tokenError(
                        arena, "No number after minus sign at position %ld",
                        p - start);
                }
            }

            if (c == '0') {
                c = *p++;
            } else {
                while (c >= '0' && c <= '9') {
                    value *= 10;
                    value += c - '0';
                    c = *p++;
                }
            }

            if (c == '.') {
                c = *p++;
                if (c >= '0' && c <= '9') {
                    double i = 0.1;
                    while (c >= '0' && c <= '9') {
                        value += (c - '0') * i;
                        i *= 0.1;
                        c = *p++;
                    }
                } else {
                    return tokenError(
                        arena, "Unterminated fractional number at position %ld",
                        p - start);
                }
            }

            if (c == 'e' || c == 'E') {
                c = *p++;
                if (c == '-') {
                    exp_sign = 0.1;
                    c = *p++;
                } else if (c == '+') {
                    c = *p++;
                }

                if (c >= '0' && c <= '9') {
                    while (c >= '0' && c <= '9') {
                        exp *= 10;
                        exp += c - '0';
                        c = *p++;
                    }
                } else {
                    return tokenError(
                        arena,
                        "Exponent part is missing a number at position %ld",
                        p - start);
                }
            }
            p--;

            for (int i = 0; i < exp; i++) {
                value *= exp_sign;
            }
            token.number = sign * value;

            return token;
        } else if (c == '"') {
            token.type = TOKEN_STRING;
            token.string = arenaAlloc(arena, JSON_STRING_SIZE);
            size_t capacity = JSON_STRING_SIZE;
            size_t size = 0;

            while ((c = *p++) != '"') {
                if (c == '\0') {
                    return tokenError(arena,
                                      "Unterminated string at position %ld",
                                      p - start);
                }

                // TODO: Add UTF-8 support
                if (c < 32 || c == 127) {
                    return tokenError(arena,
                                      "Bad control character in string literal "
                                      "at position %ld",
                                      p - start);
                }

                if (c == '\\') {
                    c = *p++;
                    switch (c) {
                        case '"':
                        case '\\':
                        case '/':
                            break;
                        case 'b':
                            c = '\b';
                            break;
                        case 'f':
                            c = '\f';
                            break;
                        case 'n':
                            c = '\n';
                            break;
                        case 'r':
                            c = '\r';
                            break;
                        case 't':
                            c = '\t';
                            break;
                        // TODO: Add Unicode
                        case 'u':
                            return tokenError(arena,
                                              "Unicode escape not implemented");
                        default:
                            return tokenError(
                                arena, "Bad escaped character at position %ld",
                                p - start);
                    }
                }

                if (size + 2 > capacity) {
                    token.string = arenaRealloc(arena, token.string, capacity,
                                                capacity * 2);
                    capacity *= 2;
                }

                token.string[size] = c;
                size++;
            }

            token.string[size] = '\0';
            return token;
        } else if (strncmp("null", p - 1, strlen("null")) == 0) {
            p += strlen("null") - 1;
            token.type = TOKEN_NULL;
            return token;
        } else if (strncmp("true", p - 1, strlen("true")) == 0) {
            p += strlen("true") - 1;
            token.type = TOKEN_BOOLEAN;
            token.boolean = true;
            return token;
        } else if (strncmp("false", p - 1, strlen("false")) == 0) {
            p += strlen("false") - 1;
            token.type = TOKEN_BOOLEAN;
            token.boolean = false;
            return token;
        } else {
            switch (c) {
                case '{':
                    token.type = TOKEN_LBRACE;
                    break;
                case '}':
                    token.type = TOKEN_RBRACE;
                    break;
                case '[':
                    token.type = TOKEN_LBRACKET;
                    break;
                case ']':
                    token.type = TOKEN_RBRACKET;
                    break;
                case ',':
                    token.type = TOKEN_COMMA;
                    break;
                case ':':
                    token.type = TOKEN_COLON;
                    break;
                default:
                    return tokenError(arena,
                                      "Unexpected token '%c' at position %ld",
                                      c, p - start);
            }
            return token;
        }
    }

    start = NULL;
    p = NULL;

    return token;
}

static JsonValue* jsonError(Arena* arena, const char* fmt, ...) {
    JsonValue* value = arenaAlloc(arena, sizeof(JsonValue));
    value->type = JSON_ERROR;
    value->string = arenaAlloc(arena, JSON_ERROR_SIZE);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(value->string, JSON_ERROR_SIZE, fmt, ap);
    va_end(ap);

    return value;
}

static JsonValue* parseArray(Arena* arena);

static JsonValue* parseObject(Arena* arena);

static JsonValue* parseValue(JsonToken token, Arena* arena) {
    JsonValue* value;
    if (token.type == TOKEN_LBRACE) {
        value = parseObject(arena);
    } else if (token.type == TOKEN_LBRACKET) {
        value = parseArray(arena);
    } else {
        value = arenaAlloc(arena, sizeof(JsonValue));
        switch (token.type) {
            case TOKEN_NULL:
                value->type = JSON_NULL;
                break;
            case TOKEN_BOOLEAN:
                value->type = JSON_BOOLEAN;
                value->boolean = token.boolean;
                break;
            case TOKEN_NUMBER:
                value->type = JSON_NUMBER;
                value->number = token.number;
                break;
            case TOKEN_STRING:
                value->type = JSON_STRING;
                value->string = token.string;
                break;
            case TOKEN_ERROR:
                value->type = JSON_ERROR;
                value->string = token.string;
                break;
            case TOKEN_EMPTY:
                return jsonError(arena, "Unexpected end");
                break;
            default:
                return jsonError(arena, "Unexpected token '%c'", token.type);
        }
    }
    return value;
}

static JsonValue* parseObject(Arena* arena) {
    JsonToken token = nextToken(NULL, arena);

    JsonObject head = {0};
    JsonObject* curr = &head;

    // TODO: Detect duplicated keys
    while (token.type == TOKEN_STRING) {
        curr->next = arenaAlloc(arena, sizeof(JsonObject));
        curr = curr->next;
        curr->next = NULL;
        curr->key = token.string;

        token = nextToken(NULL, arena);
        if (token.type != TOKEN_COLON) {
            return jsonError(arena, "Expected ':' after property name");
        }

        token = nextToken(NULL, arena);
        curr->value = parseValue(token, arena);
        if (curr->value->type == JSON_ERROR) {
            return curr->value;
        }

        token = nextToken(NULL, arena);
        if (token.type == TOKEN_RBRACE) {
            break;
        }

        if (token.type == TOKEN_COMMA) {
            token = nextToken(NULL, arena);
            if (token.type != TOKEN_STRING) {
                return jsonError(arena, "Expected double-quoted property name");
            }
        } else {
            return jsonError(arena, "Expected ',' or '}' after property value");
        }
    }

    if (token.type != TOKEN_RBRACE) {
        return jsonError(arena, "Expected property name or '}'");
    }

    JsonValue* value = arenaAlloc(arena, sizeof(JsonValue));
    value->type = JSON_OBJECT;
    value->object = head.next;
    return value;
}

static JsonValue* parseArray(Arena* arena) {
    JsonToken token = nextToken(NULL, arena);
    JsonArray* array = arenaAlloc(arena, sizeof(JsonArray));
    size_t capacity = JSON_ARRAY_SIZE;
    array->data = arenaAlloc(arena, sizeof(JsonValue*) * capacity);
    array->size = 0;

    while (token.type != TOKEN_RBRACKET) {
        if (array->size > 0) {
            if (token.type == TOKEN_COMMA) {
                token = nextToken(NULL, arena);
            } else {
                return jsonError(arena,
                                 "Expected ',' or ']' after array element");
            }
        }

        JsonValue* data = parseValue(token, arena);
        if (data->type == JSON_ERROR)
            return data;

        if (array->size + 1 > capacity) {
            size_t new_capacity = capacity * 2;
            array->data =
                arenaRealloc(arena, array->data, sizeof(JsonValue*) * capacity,
                             sizeof(JsonValue*) * new_capacity);
            capacity = new_capacity;
        }

        array->data[array->size] = data;
        array->size++;

        token = nextToken(NULL, arena);
    }

    JsonValue* value = arenaAlloc(arena, sizeof(JsonValue));
    value->type = JSON_ARRAY;
    value->array = array;
    return value;
}

JsonValue* jsonParse(const char* text, Arena* arena) {
    JsonToken token = nextToken(text, arena);
    JsonValue* value = parseValue(token, arena);
    if (value->type == JSON_ERROR)
        return value;

    // Check if any token left
    token = nextToken(NULL, arena);
    if (token.type != TOKEN_EMPTY) {
        return jsonError(arena, "Unexpected non-whitespace character");
    }
    return value;
}

JsonValue* jsonObjectFind(const JsonObject* object, const char* key) {
    while (object) {
        if (strcmp(object->key, key) == 0) {
            return object->value;
        }
        object = object->next;
    }
    return NULL;
}
