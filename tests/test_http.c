/*
 * Bolt Test Suite - HTTP Parsing Tests
 * 
 * Tests for HTTP request parsing, header extraction, and range handling.
 */

#include "minunit.h"
#include "../include/http.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * HTTP Method Parsing Tests
 *============================================================================*/

MU_TEST(test_parse_get_request) {
    const char* raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_true(req.valid);
    mu_assert_int_eq(HTTP_GET, req.method);
    mu_assert_string_eq("/index.html", req.uri);
    
    return NULL;
}

MU_TEST(test_parse_head_request) {
    const char* raw = "HEAD /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_true(req.valid);
    mu_assert_int_eq(HTTP_HEAD, req.method);
    
    return NULL;
}

MU_TEST(test_parse_post_request) {
    const char* raw = "POST /api/data HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_true(req.valid);
    mu_assert_int_eq(HTTP_POST, req.method);
    
    return NULL;
}

MU_TEST(test_parse_options_request) {
    const char* raw = "OPTIONS /api HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_true(req.valid);
    mu_assert_int_eq(HTTP_OPTIONS, req.method);
    
    return NULL;
}

MU_TEST(test_parse_unknown_method) {
    const char* raw = "DELETE /resource HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_int_eq(HTTP_UNKNOWN, req.method);
    
    return NULL;
}

/*============================================================================
 * URI Parsing Tests
 *============================================================================*/

MU_TEST(test_parse_root_uri) {
    const char* raw = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_true(req.valid);
    mu_assert_string_eq("/", req.uri);
    
    return NULL;
}

MU_TEST(test_parse_uri_with_query_string) {
    const char* raw = "GET /search?q=test&page=1 HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_true(req.valid);
    mu_assert_string_eq("/search?q=test&page=1", req.uri);
    
    return NULL;
}

MU_TEST(test_parse_uri_with_fragment) {
    const char* raw = "GET /page#section HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_true(req.valid);
    /* Fragment might be included or stripped - check it starts with /page */
    mu_check(strncmp(req.uri, "/page", 5) == 0);
    
    return NULL;
}

MU_TEST(test_parse_deep_nested_uri) {
    const char* raw = "GET /a/b/c/d/e/f/g.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_true(req.valid);
    mu_assert_string_eq("/a/b/c/d/e/f/g.html", req.uri);
    
    return NULL;
}

/*============================================================================
 * Header Extraction Tests
 *============================================================================*/

MU_TEST(test_parse_if_none_match_header) {
    const char* raw = "GET /index.html HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "If-None-Match: \"abc123\"\r\n"
                      "\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_true(req.valid);
    mu_check(strlen(req.if_none_match) > 0);
    mu_check(strstr(req.if_none_match, "abc123") != NULL);
    
    return NULL;
}

MU_TEST(test_parse_accept_encoding_header) {
    const char* raw = "GET /index.html HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "Accept-Encoding: gzip, deflate\r\n"
                      "\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_true(req.valid);
    mu_check(strstr(req.accept_encoding, "gzip") != NULL);
    
    return NULL;
}

/*============================================================================
 * Range Header Parsing Tests
 *============================================================================*/

MU_TEST(test_parse_range_bytes_start_end) {
    /* bytes=0-499 means bytes 0 through 499 (first 500 bytes) */
    HttpRange range = http_parse_range("bytes=0-499", 1000);
    
    mu_assert_true(range.valid);
    mu_assert_size_eq(0, range.start);
    mu_assert_size_eq(499, range.end);
    
    return NULL;
}

MU_TEST(test_parse_range_bytes_start_only) {
    /* bytes=500- means from byte 500 to end */
    HttpRange range = http_parse_range("bytes=500-", 1000);
    
    mu_assert_true(range.valid);
    mu_assert_size_eq(500, range.start);
    mu_assert_size_eq(999, range.end);  /* File is 1000 bytes, so last byte is 999 */
    
    return NULL;
}

MU_TEST(test_parse_range_bytes_suffix) {
    /* bytes=-500 means last 500 bytes */
    HttpRange range = http_parse_range("bytes=-500", 1000);
    
    mu_assert_true(range.valid);
    mu_assert_size_eq(500, range.start);  /* 1000 - 500 = 500 */
    mu_assert_size_eq(999, range.end);
    
    return NULL;
}

MU_TEST(test_parse_range_beyond_file_size) {
    /* End beyond file size should be clamped */
    HttpRange range = http_parse_range("bytes=0-9999", 1000);
    
    mu_assert_true(range.valid);
    mu_assert_size_eq(0, range.start);
    mu_assert_size_eq(999, range.end);
    
    return NULL;
}

MU_TEST(test_parse_range_start_beyond_file) {
    /* Start beyond file size is invalid */
    HttpRange range = http_parse_range("bytes=2000-", 1000);
    
    mu_assert_false(range.valid);
    
    return NULL;
}

MU_TEST(test_parse_range_invalid_format) {
    /* Missing "bytes=" prefix */
    HttpRange range = http_parse_range("0-499", 1000);
    
    mu_assert_false(range.valid);
    
    return NULL;
}

MU_TEST(test_parse_range_null_header) {
    HttpRange range = http_parse_range(NULL, 1000);
    
    mu_assert_false(range.valid);
    
    return NULL;
}

MU_TEST(test_parse_range_zero_file_size) {
    HttpRange range = http_parse_range("bytes=0-499", 0);
    
    mu_assert_false(range.valid);
    
    return NULL;
}

/*============================================================================
 * Malformed Request Tests
 *============================================================================*/

MU_TEST(test_parse_empty_request) {
    HttpRequest req = http_parse_request("", 0);
    
    mu_assert_false(req.valid);
    
    return NULL;
}

MU_TEST(test_parse_null_request) {
    HttpRequest req = http_parse_request(NULL, 0);
    
    mu_assert_false(req.valid);
    
    return NULL;
}

MU_TEST(test_parse_incomplete_request_line) {
    const char* raw = "GET";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    mu_assert_false(req.valid);
    
    return NULL;
}

MU_TEST(test_parse_missing_http_version) {
    const char* raw = "GET /index.html\r\n\r\n";
    HttpRequest req = http_parse_request(raw, strlen(raw));
    
    /* Should either be invalid or handle gracefully */
    /* Depending on implementation, this might be valid or invalid */
    
    return NULL;  /* Just don't crash */
}

/*============================================================================
 * Status Text Tests
 *============================================================================*/

MU_TEST(test_status_text_200) {
    const char* text = http_status_text(HTTP_200_OK);
    mu_assert_string_eq("OK", text);
    return NULL;
}

MU_TEST(test_status_text_404) {
    const char* text = http_status_text(HTTP_404_NOT_FOUND);
    mu_assert_string_eq("Not Found", text);
    return NULL;
}

MU_TEST(test_status_text_206) {
    const char* text = http_status_text(HTTP_206_PARTIAL_CONTENT);
    mu_assert_string_eq("Partial Content", text);
    return NULL;
}

MU_TEST(test_status_text_500) {
    const char* text = http_status_text(HTTP_500_INTERNAL_ERROR);
    mu_assert_string_eq("Internal Server Error", text);
    return NULL;
}

/*============================================================================
 * Test Suite Runner
 *============================================================================*/

void test_suite_http(void) {
    /* Method parsing */
    MU_RUN_TEST(test_parse_get_request);
    MU_RUN_TEST(test_parse_head_request);
    MU_RUN_TEST(test_parse_post_request);
    MU_RUN_TEST(test_parse_options_request);
    MU_RUN_TEST(test_parse_unknown_method);
    
    /* URI parsing */
    MU_RUN_TEST(test_parse_root_uri);
    MU_RUN_TEST(test_parse_uri_with_query_string);
    MU_RUN_TEST(test_parse_uri_with_fragment);
    MU_RUN_TEST(test_parse_deep_nested_uri);
    
    /* Header extraction */
    MU_RUN_TEST(test_parse_if_none_match_header);
    MU_RUN_TEST(test_parse_accept_encoding_header);
    
    /* Range header parsing */
    MU_RUN_TEST(test_parse_range_bytes_start_end);
    MU_RUN_TEST(test_parse_range_bytes_start_only);
    MU_RUN_TEST(test_parse_range_bytes_suffix);
    MU_RUN_TEST(test_parse_range_beyond_file_size);
    MU_RUN_TEST(test_parse_range_start_beyond_file);
    MU_RUN_TEST(test_parse_range_invalid_format);
    MU_RUN_TEST(test_parse_range_null_header);
    MU_RUN_TEST(test_parse_range_zero_file_size);
    
    /* Malformed requests */
    MU_RUN_TEST(test_parse_empty_request);
    MU_RUN_TEST(test_parse_null_request);
    MU_RUN_TEST(test_parse_incomplete_request_line);
    MU_RUN_TEST(test_parse_missing_http_version);
    
    /* Status text */
    MU_RUN_TEST(test_status_text_200);
    MU_RUN_TEST(test_status_text_404);
    MU_RUN_TEST(test_status_text_206);
    MU_RUN_TEST(test_status_text_500);
}

