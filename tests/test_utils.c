/*
 * Bolt Test Suite - Utils Tests (Security-Critical)
 * 
 * Tests for path sanitization, URL decoding, and security validation.
 * These are critical security tests - any failure here could indicate a vulnerability.
 */

#include "minunit.h"
#include "../include/utils.h"
#include "../include/bolt.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Path Traversal Attack Tests
 *============================================================================*/

MU_TEST(test_sanitize_rejects_dot_dot_slash) {
    char out[512];
    /* Basic directory traversal */
    mu_assert_false(utils_sanitize_path("/../etc/passwd", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/../../etc/passwd", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/../../../etc/passwd", out, sizeof(out)));
    return NULL;
}

MU_TEST(test_sanitize_rejects_dot_dot_backslash) {
    char out[512];
    /* Windows-style directory traversal */
    mu_assert_false(utils_sanitize_path("/..\\etc\\passwd", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/..\\..\\Windows\\System32", out, sizeof(out)));
    return NULL;
}

MU_TEST(test_sanitize_rejects_encoded_traversal) {
    char out[512];
    /* URL-encoded directory traversal */
    mu_assert_false(utils_sanitize_path("/%2e%2e/etc/passwd", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/%2e%2e%2fetc%2fpasswd", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/%2E%2E/etc/passwd", out, sizeof(out)));
    return NULL;
}

MU_TEST(test_sanitize_rejects_double_encoded_traversal) {
    char out[512];
    /* Double URL-encoded (decoded becomes %2e%2e which decodes to ..) */
    mu_assert_false(utils_sanitize_path("/%252e%252e/etc/passwd", out, sizeof(out)));
    return NULL;
}

MU_TEST(test_sanitize_rejects_mixed_slashes) {
    char out[512];
    /* Mixed forward and back slashes */
    mu_assert_false(utils_sanitize_path("/..\\../etc/passwd", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/../..\\etc/passwd", out, sizeof(out)));
    return NULL;
}

/*============================================================================
 * Null Byte Injection Tests
 *============================================================================*/

MU_TEST(test_sanitize_handles_null_byte) {
    char out[512];
    /* Null byte injection (truncation attack) */
    /* The %00 should be decoded and rejected */
    mu_assert_false(utils_sanitize_path("/index.html%00.jpg", out, sizeof(out)));
    return NULL;
}

/*============================================================================
 * Windows-Specific Attack Tests
 *============================================================================*/

MU_TEST(test_sanitize_rejects_drive_letter) {
    char out[512];
    /* Windows drive letter access */
    mu_assert_false(utils_sanitize_path("/C:/Windows/System32", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/c:\\Windows\\System32", out, sizeof(out)));
    return NULL;
}

MU_TEST(test_sanitize_rejects_unc_path) {
    char out[512];
    /* UNC path access */
    mu_assert_false(utils_sanitize_path("//server/share", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("\\\\server\\share", out, sizeof(out)));
    return NULL;
}

MU_TEST(test_sanitize_rejects_hidden_files) {
    char out[512];
    /* Hidden files (starting with dot) */
    mu_assert_false(utils_sanitize_path("/.htaccess", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/.git/config", out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/.env", out, sizeof(out)));
    return NULL;
}

/*============================================================================
 * Valid Path Tests
 *============================================================================*/

MU_TEST(test_sanitize_allows_root) {
    char out[512];
    /* Root path should be allowed */
    mu_assert_true(utils_sanitize_path("/", out, sizeof(out)));
    return NULL;
}

MU_TEST(test_sanitize_allows_simple_file) {
    char out[512];
    /* Simple file in root */
    mu_assert_true(utils_sanitize_path("/index.html", out, sizeof(out)));
    mu_check(strstr(out, "index.html") != NULL);
    return NULL;
}

MU_TEST(test_sanitize_allows_nested_path) {
    char out[512];
    /* Nested directory structure */
    mu_assert_true(utils_sanitize_path("/css/style.css", out, sizeof(out)));
    mu_check(strstr(out, "css") != NULL);
    mu_check(strstr(out, "style.css") != NULL);
    return NULL;
}

MU_TEST(test_sanitize_allows_deep_nested_path) {
    char out[512];
    /* Deeply nested path */
    mu_assert_true(utils_sanitize_path("/assets/images/icons/favicon.ico", out, sizeof(out)));
    mu_check(strstr(out, "favicon.ico") != NULL);
    return NULL;
}

MU_TEST(test_sanitize_allows_url_encoded_spaces) {
    char out[512];
    /* URL-encoded spaces in valid filename */
    mu_assert_true(utils_sanitize_path("/my%20file.txt", out, sizeof(out)));
    mu_check(strstr(out, "my file.txt") != NULL);
    return NULL;
}

/*============================================================================
 * URL Decoding Tests
 *============================================================================*/

MU_TEST(test_url_decode_basic) {
    char str[] = "hello%20world";
    utils_url_decode(str);
    mu_assert_string_eq("hello world", str);
    return NULL;
}

MU_TEST(test_url_decode_plus_sign) {
    char str[] = "hello+world";
    utils_url_decode(str);
    mu_assert_string_eq("hello world", str);
    return NULL;
}

MU_TEST(test_url_decode_multiple) {
    char str[] = "%2Fpath%2Fto%2Ffile";
    utils_url_decode(str);
    mu_assert_string_eq("/path/to/file", str);
    return NULL;
}

MU_TEST(test_url_decode_mixed_case_hex) {
    char str[] = "%2f%2F";  /* Both lowercase and uppercase */
    utils_url_decode(str);
    mu_assert_string_eq("//", str);
    return NULL;
}

/*============================================================================
 * Edge Case Tests
 *============================================================================*/

MU_TEST(test_sanitize_empty_path) {
    char out[512];
    /* Empty path should work (becomes root) */
    mu_assert_true(utils_sanitize_path("", out, sizeof(out)));
    return NULL;
}

MU_TEST(test_sanitize_rejects_very_long_path) {
    char out[512];
    /* Very long path should be rejected */
    char long_path[4096];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[0] = '/';
    long_path[sizeof(long_path) - 1] = '\0';
    mu_assert_false(utils_sanitize_path(long_path, out, sizeof(out)));
    return NULL;
}

MU_TEST(test_sanitize_null_inputs) {
    char out[512];
    /* NULL inputs should be handled gracefully */
    mu_assert_false(utils_sanitize_path(NULL, out, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/test", NULL, sizeof(out)));
    mu_assert_false(utils_sanitize_path("/test", out, 0));
    return NULL;
}

/*============================================================================
 * File Info Tests
 *============================================================================*/

MU_TEST(test_get_extension_basic) {
    const char* ext;
    
    ext = utils_get_extension("file.txt");
    mu_assert_string_eq("txt", ext);
    
    ext = utils_get_extension("file.html");
    mu_assert_string_eq("html", ext);
    
    ext = utils_get_extension("archive.tar.gz");
    mu_assert_string_eq("gz", ext);
    
    return NULL;
}

MU_TEST(test_get_extension_no_extension) {
    const char* ext;
    
    ext = utils_get_extension("README");
    mu_assert_string_eq("", ext);
    
    ext = utils_get_extension("Makefile");
    mu_assert_string_eq("", ext);
    
    return NULL;
}

MU_TEST(test_get_extension_hidden_file) {
    const char* ext;
    
    /* Hidden files with extensions */
    ext = utils_get_extension(".gitignore");
    mu_assert_string_eq("", ext);
    
    return NULL;
}

/*============================================================================
 * Test Suite Runner
 *============================================================================*/

void test_suite_utils(void) {
    /* Path traversal attacks */
    MU_RUN_TEST(test_sanitize_rejects_dot_dot_slash);
    MU_RUN_TEST(test_sanitize_rejects_dot_dot_backslash);
    MU_RUN_TEST(test_sanitize_rejects_encoded_traversal);
    MU_RUN_TEST(test_sanitize_rejects_double_encoded_traversal);
    MU_RUN_TEST(test_sanitize_rejects_mixed_slashes);
    
    /* Null byte injection */
    MU_RUN_TEST(test_sanitize_handles_null_byte);
    
    /* Windows-specific attacks */
    MU_RUN_TEST(test_sanitize_rejects_drive_letter);
    MU_RUN_TEST(test_sanitize_rejects_unc_path);
    MU_RUN_TEST(test_sanitize_rejects_hidden_files);
    
    /* Valid paths */
    MU_RUN_TEST(test_sanitize_allows_root);
    MU_RUN_TEST(test_sanitize_allows_simple_file);
    MU_RUN_TEST(test_sanitize_allows_nested_path);
    MU_RUN_TEST(test_sanitize_allows_deep_nested_path);
    MU_RUN_TEST(test_sanitize_allows_url_encoded_spaces);
    
    /* URL decoding */
    MU_RUN_TEST(test_url_decode_basic);
    MU_RUN_TEST(test_url_decode_plus_sign);
    MU_RUN_TEST(test_url_decode_multiple);
    MU_RUN_TEST(test_url_decode_mixed_case_hex);
    
    /* Edge cases */
    MU_RUN_TEST(test_sanitize_empty_path);
    MU_RUN_TEST(test_sanitize_rejects_very_long_path);
    MU_RUN_TEST(test_sanitize_null_inputs);
    
    /* File info */
    MU_RUN_TEST(test_get_extension_basic);
    MU_RUN_TEST(test_get_extension_no_extension);
    MU_RUN_TEST(test_get_extension_hidden_file);
}

