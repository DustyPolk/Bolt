/*
 * MinUnit - Minimal Unit Testing Framework for C
 * Based on: http://www.jera.com/techinfo/jtns/jtn002.html
 * Extended with additional assertions and reporting.
 */

#ifndef MINUNIT_H
#define MINUNIT_H

#include <stdio.h>
#include <string.h>

/* Test counters */
extern int mu_tests_run;
extern int mu_tests_passed;
extern int mu_tests_failed;
extern int mu_assertions;
extern const char* mu_last_error;

/* Colors for terminal output (Windows compatible) */
#ifdef _WIN32
#define MU_COLOR_RED ""
#define MU_COLOR_GREEN ""
#define MU_COLOR_YELLOW ""
#define MU_COLOR_RESET ""
#else
#define MU_COLOR_RED "\x1b[31m"
#define MU_COLOR_GREEN "\x1b[32m"
#define MU_COLOR_YELLOW "\x1b[33m"
#define MU_COLOR_RESET "\x1b[0m"
#endif

/* Basic assertion - returns error message on failure */
#define mu_assert(message, test) do { \
    mu_assertions++; \
    if (!(test)) { \
        mu_last_error = message; \
        return message; \
    } \
} while (0)

/* Check assertion - auto-generates message */
#define mu_check(test) do { \
    mu_assertions++; \
    if (!(test)) { \
        mu_last_error = "Check failed: " #test; \
        return "Check failed: " #test; \
    } \
} while (0)

/* String equality assertion */
#define mu_assert_string_eq(expected, actual) do { \
    mu_assertions++; \
    if (strcmp((expected), (actual)) != 0) { \
        static char mu_msg_buffer[256]; \
        snprintf(mu_msg_buffer, sizeof(mu_msg_buffer), \
            "Expected \"%s\" but got \"%s\"", (expected), (actual)); \
        mu_last_error = mu_msg_buffer; \
        return mu_msg_buffer; \
    } \
} while (0)

/* Integer equality assertion */
#define mu_assert_int_eq(expected, actual) do { \
    mu_assertions++; \
    if ((expected) != (actual)) { \
        static char mu_msg_buffer[256]; \
        snprintf(mu_msg_buffer, sizeof(mu_msg_buffer), \
            "Expected %d but got %d", (int)(expected), (int)(actual)); \
        mu_last_error = mu_msg_buffer; \
        return mu_msg_buffer; \
    } \
} while (0)

/* Size equality assertion */
#define mu_assert_size_eq(expected, actual) do { \
    mu_assertions++; \
    if ((expected) != (actual)) { \
        static char mu_msg_buffer[256]; \
        snprintf(mu_msg_buffer, sizeof(mu_msg_buffer), \
            "Expected %zu but got %zu", (size_t)(expected), (size_t)(actual)); \
        mu_last_error = mu_msg_buffer; \
        return mu_msg_buffer; \
    } \
} while (0)

/* Boolean assertions */
#define mu_assert_true(actual) mu_assert("Expected true but got false", (actual))
#define mu_assert_false(actual) mu_assert("Expected false but got true", !(actual))

/* Pointer assertions */
#define mu_assert_null(actual) mu_assert("Expected NULL but got non-NULL", (actual) == NULL)
#define mu_assert_not_null(actual) mu_assert("Expected non-NULL but got NULL", (actual) != NULL)

/* Define a test function */
#define MU_TEST(name) static const char* name(void)

/* Run a single test */
#define MU_RUN_TEST(test) do { \
    printf("[ RUN      ] %s\n", #test); \
    mu_last_error = NULL; \
    const char* message = test(); \
    mu_tests_run++; \
    if (message) { \
        printf("[  FAILED  ] %s\n", #test); \
        printf("             %s\n", message); \
        mu_tests_failed++; \
    } else { \
        printf("[       OK ] %s\n", #test); \
        mu_tests_passed++; \
    } \
} while (0)

/* Run a test suite (group of tests) */
#define MU_RUN_SUITE(suite) do { \
    printf("\n[----------] Running %s\n", #suite); \
    suite(); \
    printf("[----------] Finished %s\n", #suite); \
} while (0)

/* Print test summary */
#define MU_REPORT() do { \
    printf("\n[==========] %d test(s) ran\n", mu_tests_run); \
    if (mu_tests_passed > 0) { \
        printf("[  PASSED  ] %d test(s)\n", mu_tests_passed); \
    } \
    if (mu_tests_failed > 0) { \
        printf("[  FAILED  ] %d test(s)\n", mu_tests_failed); \
    } \
    printf("[  ASSERTS ] %d assertion(s)\n", mu_assertions); \
} while (0)

/* Reset counters (for running multiple test files) */
#define MU_RESET() do { \
    mu_tests_run = 0; \
    mu_tests_passed = 0; \
    mu_tests_failed = 0; \
    mu_assertions = 0; \
    mu_last_error = NULL; \
} while (0)

/* Return value for main() */
#define MU_EXIT_CODE (mu_tests_failed > 0 ? 1 : 0)

#endif /* MINUNIT_H */

