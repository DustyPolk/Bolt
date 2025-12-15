/*
 * Bolt Test Suite - Security Tests
 * 
 * Tests for specific security vulnerabilities and regression testing.
 */

#include "minunit.h"
#include "../include/utils.h"
#include <string.h>

/*============================================================================
 * Windows Device Name Tests
 *============================================================================*/

MU_TEST(test_reject_device_names) {
    char out[512];
    
    /* Exact matches */
    mu_assert_false(utils_sanitize_path("/CON", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/PRN", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/AUX", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/NUL", out, sizeof(out)));
    
    /* COM ports */
    mu_assert_false(utils_sanitize_path("/COM1", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/COM9", out, sizeof(out)));
    
    /* LPT ports */
    mu_assert_false(utils_sanitize_path("/LPT1", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/LPT9", out, sizeof(out)));
    
    /* Case insensitivity */
    mu_assert_false(utils_sanitize_path("/con", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/Aux", out, sizeof(out)));
    
    /* With extensions */
    mu_assert_false(utils_sanitize_path("/CON.txt", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/prn.html", out, sizeof(out)));
    
    /* In subdirectories */
    mu_assert_false(utils_sanitize_path("/folder/CON", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/folder/COM1.txt", out, sizeof(out)));
    
    return NULL;
}

MU_TEST(test_allow_safe_names_starting_with_device) {
    char out[512];
    
    /* "console" starts with "con" but is safe */
    mu_assert_true(utils_sanitize_path("/console.txt", out, sizeof(out)));
    
    /* "context" starts with "con" */
    mu_assert_true(utils_sanitize_path("/context", out, sizeof(out)));
    
    /* "auxiliary" starts with "aux" */
    mu_assert_true(utils_sanitize_path("/auxiliary", out, sizeof(out)));
    
    return NULL;
}

/*============================================================================
 * Alternate Data Stream (ADS) Tests
 *============================================================================*/

MU_TEST(test_reject_ads) {
    char out[512];
    
    /* Basic ADS pattern */
    mu_assert_false(utils_sanitize_path("/file.txt:stream", out, sizeof(out)));
    
    /* Root ADS */
    mu_assert_false(utils_sanitize_path("/:stream", out, sizeof(out)));
    
    return NULL;
}

/*============================================================================
 * Null Byte Injection (Re-verify)
 *============================================================================*/

MU_TEST(test_reject_null_byte_security) {
    char out[512];
    mu_assert_false(utils_sanitize_path("/index.html%00.jpg", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/%00", out, sizeof(out)));
    return NULL;
}

/*============================================================================
 * Test Suite Runner
 *============================================================================*/

void test_suite_security(void) {
    MU_RUN_TEST(test_reject_device_names);
    MU_RUN_TEST(test_allow_safe_names_starting_with_device);
    MU_RUN_TEST(test_reject_ads);
    MU_RUN_TEST(test_reject_null_byte_security);
}
