/*
 * Bolt Test Suite - Main Test Runner
 * 
 * Runs all unit tests and integration tests for the Bolt HTTP server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>

#include "minunit.h"

/* External test suite declarations */
extern void test_suite_utils(void);
extern void test_suite_http(void);
extern void test_suite_mime(void);
extern void test_suite_rewrite(void);
extern void test_suite_config(void);
extern void test_suite_pool(void);
extern void test_suite_cache(void);
extern void test_suite_server(void);

/*
 * Initialize Winsock for network tests.
 */
static int init_winsock(void) {
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        printf("WSAStartup failed: %d\n", result);
        return -1;
    }
    return 0;
}

/*
 * Cleanup Winsock.
 */
static void cleanup_winsock(void) {
    WSACleanup();
}

/*
 * Print banner.
 */
static void print_banner(void) {
    printf("\n");
    printf("  ============================================\n");
    printf("  ⚡ BOLT Test Suite\n");
    printf("  ============================================\n");
    printf("\n");
}

/*
 * Main entry point.
 */
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    print_banner();
    
    /* Initialize Winsock for network tests */
    if (init_winsock() != 0) {
        return 1;
    }
    
    printf("[==========] Running all test suites\n");
    
    /* Run unit test suites */
    MU_RUN_SUITE(test_suite_utils);
    MU_RUN_SUITE(test_suite_http);
    MU_RUN_SUITE(test_suite_mime);
    MU_RUN_SUITE(test_suite_rewrite);
    MU_RUN_SUITE(test_suite_config);
    MU_RUN_SUITE(test_suite_pool);
    MU_RUN_SUITE(test_suite_cache);
    
    /* Run integration tests */
    MU_RUN_SUITE(test_suite_server);
    
    /* Print summary */
    MU_REPORT();
    
    /* Cleanup */
    cleanup_winsock();
    
    printf("\n");
    if (MU_EXIT_CODE == 0) {
        printf("  All tests passed!\n");
    } else {
        printf("  Some tests failed. Please review the output above.\n");
    }
    printf("\n");
    
    return MU_EXIT_CODE;
}

