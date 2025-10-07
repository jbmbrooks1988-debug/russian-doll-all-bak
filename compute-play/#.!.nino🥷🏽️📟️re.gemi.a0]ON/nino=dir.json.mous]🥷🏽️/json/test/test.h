#ifndef TEST_H
#define TEST_H

#include <stdbool.h>
#include <stdio.h>

typedef struct Test {
    bool failed;
    const char* desc;
    void (*func)(bool* result);
} Test;

#define TEST(name, desc)                    \
    static void name##_func(bool* result);  \
    Test name = {false, desc, name##_func}; \
    static void name##_func(bool* result)

#define ASSERT(cond)         \
    do {                     \
        if (!(cond)) {       \
            *result = false; \
            return;          \
        }                    \
    } while (0)

#define RUN_ALL_TESTS(tests, name)                                      \
    do {                                                                \
        fprintf(stderr, "[==========] Running " name " tests\n");       \
        for (size_t i = 0; i < sizeof(tests) / sizeof(Test*); i++) {    \
            bool result = true;                                         \
            tests[i]->func(&result);                                    \
            if (result) {                                               \
                fprintf(stderr, "[\x1b[38;5;2m  PASSED  \x1b[0m] %s\n", \
                        tests[i]->desc);                                \
            } else {                                                    \
                fprintf(stderr, "[\x1b[38;5;1m  FAILED  \x1b[0m] %s\n", \
                        tests[i]->desc);                                \
            }                                                           \
        }                                                               \
    } while (0)

#endif
