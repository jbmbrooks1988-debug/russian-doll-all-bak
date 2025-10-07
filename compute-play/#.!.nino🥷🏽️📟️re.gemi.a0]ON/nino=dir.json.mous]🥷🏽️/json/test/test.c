#include "test.h"

#include <string.h>

#include "json.h"

TEST(arena1, "arenaAlloc") {
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    uint8_t* ptr1 = arenaAlloc(&arena, 10);
    uint8_t* ptr2 = arenaAlloc(&arena, 10);
    ASSERT(ptr1 != ptr2);
    for (int i = 0; i < 10; i++) {
        ptr1[i] = 87;
        ptr2[i] = 63;
    }
    for (int i = 0; i < 10; i++) {
        ASSERT(ptr1[i] == 87);
        ASSERT(ptr2[i] == 63);
    }
    arenaDeinit(&arena);
}

TEST(arena2, "arenaReset") {
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    uint8_t* ptr1 = arenaAlloc(&arena, 10);
    uint8_t* ptr2 = arenaAlloc(&arena, 10);
    ASSERT(ptr1 != ptr2);
    arenaReset(&arena);
    uint8_t* ptr3 = arenaAlloc(&arena, 10);
    ASSERT(ptr1 == ptr3);
    arenaDeinit(&arena);
}

TEST(arena3, "arenaRealloc") {
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    uint8_t* ptr1 = arenaAlloc(&arena, 10);
    for (int i = 0; i < 10; i++) {
        ptr1[i] = 87;
    }
    // in-place realloc
    uint8_t* ptr2 = arenaRealloc(&arena, ptr1, 10, 30);
    ASSERT(ptr1 == ptr2);
    arenaAlloc(&arena, 10);
    // not in-place realloc
    uint8_t* ptr3 = arenaRealloc(&arena, ptr1, 30, 40);
    ASSERT(ptr1 != ptr3);
    for (int i = 0; i < 10; i++) {
        ASSERT(ptr3[i] == 87);
    }
    arenaDeinit(&arena);
}

Test* arena_tests[] = {
    &arena1,
    &arena2,
    &arena3,
};

TEST(good_json1, "Parse null") {
    const char* buffer = "null";
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value = jsonParse(buffer, &arena);
    ASSERT(value->type == JSON_NULL);
    arenaDeinit(&arena);
}

TEST(good_json2, "Parse array") {
    const char* buffer = "[null, null, null]";
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value = jsonParse(buffer, &arena);
    ASSERT(value->type == JSON_ARRAY);
    ASSERT(value->array->size == 3);

    ASSERT(value->array->data[0]->type == JSON_NULL);
    ASSERT(value->array->data[1]->type == JSON_NULL);
    ASSERT(value->array->data[2]->type == JSON_NULL);
    arenaDeinit(&arena);
}

#define EPSILON 1e-10
static int compareNumber(double a, double b, double epsilon) {
    double diff = (a > b) ? (a - b) : (b - a);

    if (diff < epsilon) {
        return 0;
    } else if (a < b) {
        return -1;
    } else {
        return 1;
    }
}

TEST(good_json3, "Parse number") {
    const char* buffer = "[3, 3.14, 1e5, -5.2, -1E-4, 3.1e+4]";
    double gold[] = {3, 3.14, 1e5, -5.2, -1E-4, 3.1e+4};
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value = jsonParse(buffer, &arena);
    ASSERT(value->type == JSON_ARRAY);
    ASSERT(value->array->size == 6);
    for (size_t i = 0; i < value->array->size; i++) {
        ASSERT(value->array->data[i]->type == JSON_NUMBER);
        ASSERT(compareNumber(value->array->data[i]->number, gold[i], EPSILON) ==
               0);
    }
    arenaDeinit(&arena);
}

TEST(good_json4, "Parse boolean") {
    const char* buffer = "[true, false]";
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value = jsonParse(buffer, &arena);
    ASSERT(value->type == JSON_ARRAY);
    ASSERT(value->array->size == 2);

    ASSERT(value->array->data[0]->type == JSON_BOOLEAN);
    ASSERT(value->array->data[0]->boolean == true);
    ASSERT(value->array->data[1]->type == JSON_BOOLEAN);
    ASSERT(value->array->data[1]->boolean == false);
    arenaDeinit(&arena);
}

TEST(good_json5, "Parse string") {
    const char* buffer = "[\"string\", \"newline\\n\", \"tab\\t\"]";
    const char* gold[] = {"string", "newline\n", "tab\t"};
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value = jsonParse(buffer, &arena);
    ASSERT(value->type == JSON_ARRAY);
    ASSERT(value->array->size == 3);
    for (size_t i = 0; i < value->array->size; i++) {
        ASSERT(value->array->data[i]->type == JSON_STRING);
        ASSERT(strcmp(value->array->data[i]->string, gold[i]) == 0);
    }
    arenaDeinit(&arena);
}

TEST(good_json6, "Parse object") {
    const char* buffer = "{\"key1\": \"string\", \"key2\": 5}";
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value = jsonParse(buffer, &arena);
    ASSERT(value->type == JSON_OBJECT);
    JsonValue* key1 = jsonObjectFind(value->object, "key1");
    ASSERT(key1->type == JSON_STRING);
    ASSERT(strcmp(key1->string, "string") == 0);
    JsonValue* key2 = jsonObjectFind(value->object, "key2");
    ASSERT(key2->type == JSON_NUMBER);
    ASSERT(key2->number == 5);
    arenaDeinit(&arena);
}

Test* json_good_tests[] = {
    &good_json1, &good_json2, &good_json3,
    &good_json4, &good_json5, &good_json6,
};

TEST(bad_json1, "Extra comma") {
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value;
    value = jsonParse("{\"test\":3,}", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("[1,2,3,]", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("[1,2,3],", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("[,\"test\"]", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("[1,2,,]", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaDeinit(&arena);
}

TEST(bad_json2, "Bad string") {
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value;
    value = jsonParse("\'single qoute\'", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("\"tabs\tin\tstring\"", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("\"Invalid \\x32\"", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaDeinit(&arena);
}

TEST(bad_json3, "Bad number") {
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value;
    value = jsonParse("0x32", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("032", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("0e+-1", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("0e+", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaDeinit(&arena);
}

TEST(bad_json4, "Missing token") {
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value;
    value = jsonParse("{\"key\" 5}", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("[1,2,3", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("{\"key\": 5", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaReset(&arena);
    value = jsonParse("[\"test]", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaDeinit(&arena);
}

TEST(bad_json5, "Empty input") {
    Arena arena;
    arenaInit(&arena, 1 << 10, NULL);
    JsonValue* value = jsonParse("", &arena);
    ASSERT(value->type == JSON_ERROR);
    arenaDeinit(&arena);
}

Test* json_bad_tests[] = {
    &bad_json1, &bad_json2, &bad_json3, &bad_json4, &bad_json5,
};

int main(void) {
    RUN_ALL_TESTS(arena_tests, "arena");
    RUN_ALL_TESTS(json_good_tests, "good JSON");
    RUN_ALL_TESTS(json_bad_tests, "bad JSON");
    return 0;
}
