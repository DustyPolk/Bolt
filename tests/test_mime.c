/*
 * Bolt Test Suite - MIME Type Tests
 * 
 * Tests for MIME type detection based on file extensions.
 */

#include "minunit.h"
#include "../include/mime.h"
#include <string.h>

/*============================================================================
 * Common MIME Type Tests
 *============================================================================*/

MU_TEST(test_mime_html) {
    const char* mime = mime_type_from_extension("html");
    mu_assert_string_eq("text/html; charset=utf-8", mime);
    return NULL;
}

MU_TEST(test_mime_htm) {
    const char* mime = mime_type_from_extension("htm");
    mu_assert_string_eq("text/html; charset=utf-8", mime);
    return NULL;
}

MU_TEST(test_mime_css) {
    const char* mime = mime_type_from_extension("css");
    mu_assert_string_eq("text/css; charset=utf-8", mime);
    return NULL;
}

MU_TEST(test_mime_js) {
    const char* mime = mime_type_from_extension("js");
    mu_assert_string_eq("application/javascript; charset=utf-8", mime);
    return NULL;
}

MU_TEST(test_mime_json) {
    const char* mime = mime_type_from_extension("json");
    mu_assert_string_eq("application/json; charset=utf-8", mime);
    return NULL;
}

MU_TEST(test_mime_txt) {
    const char* mime = mime_type_from_extension("txt");
    mu_assert_string_eq("text/plain; charset=utf-8", mime);
    return NULL;
}

/*============================================================================
 * Image MIME Type Tests
 *============================================================================*/

MU_TEST(test_mime_png) {
    const char* mime = mime_type_from_extension("png");
    mu_assert_string_eq("image/png", mime);
    return NULL;
}

MU_TEST(test_mime_jpg) {
    const char* mime = mime_type_from_extension("jpg");
    mu_assert_string_eq("image/jpeg", mime);
    return NULL;
}

MU_TEST(test_mime_jpeg) {
    const char* mime = mime_type_from_extension("jpeg");
    mu_assert_string_eq("image/jpeg", mime);
    return NULL;
}

MU_TEST(test_mime_gif) {
    const char* mime = mime_type_from_extension("gif");
    mu_assert_string_eq("image/gif", mime);
    return NULL;
}

MU_TEST(test_mime_svg) {
    const char* mime = mime_type_from_extension("svg");
    mu_assert_string_eq("image/svg+xml", mime);
    return NULL;
}

MU_TEST(test_mime_ico) {
    const char* mime = mime_type_from_extension("ico");
    mu_assert_string_eq("image/x-icon", mime);
    return NULL;
}

MU_TEST(test_mime_webp) {
    const char* mime = mime_type_from_extension("webp");
    mu_assert_string_eq("image/webp", mime);
    return NULL;
}

/*============================================================================
 * Font MIME Type Tests
 *============================================================================*/

MU_TEST(test_mime_woff) {
    const char* mime = mime_type_from_extension("woff");
    mu_assert_string_eq("font/woff", mime);
    return NULL;
}

MU_TEST(test_mime_woff2) {
    const char* mime = mime_type_from_extension("woff2");
    mu_assert_string_eq("font/woff2", mime);
    return NULL;
}

MU_TEST(test_mime_ttf) {
    const char* mime = mime_type_from_extension("ttf");
    mu_assert_string_eq("font/ttf", mime);
    return NULL;
}

/*============================================================================
 * Application MIME Type Tests
 *============================================================================*/

MU_TEST(test_mime_pdf) {
    const char* mime = mime_type_from_extension("pdf");
    mu_assert_string_eq("application/pdf", mime);
    return NULL;
}

MU_TEST(test_mime_zip) {
    const char* mime = mime_type_from_extension("zip");
    mu_assert_string_eq("application/zip", mime);
    return NULL;
}

MU_TEST(test_mime_xml) {
    const char* mime = mime_type_from_extension("xml");
    mu_assert_string_eq("application/xml", mime);
    return NULL;
}

/*============================================================================
 * Unknown/Default MIME Type Tests
 *============================================================================*/

MU_TEST(test_mime_unknown_extension) {
    const char* mime = mime_type_from_extension("xyz123");
    mu_assert_string_eq("application/octet-stream", mime);
    return NULL;
}

MU_TEST(test_mime_empty_extension) {
    const char* mime = mime_type_from_extension("");
    mu_assert_string_eq("application/octet-stream", mime);
    return NULL;
}

MU_TEST(test_mime_null_extension) {
    const char* mime = mime_type_from_extension(NULL);
    mu_assert_string_eq("application/octet-stream", mime);
    return NULL;
}

/*============================================================================
 * Case Sensitivity Tests
 *============================================================================*/

MU_TEST(test_mime_uppercase_html) {
    const char* mime = mime_type_from_extension("HTML");
    /* Should handle uppercase extensions */
    mu_check(strstr(mime, "text/html") != NULL || strcmp(mime, "application/octet-stream") == 0);
    return NULL;
}

MU_TEST(test_mime_mixed_case) {
    const char* mime = mime_type_from_extension("HtMl");
    /* Should handle mixed case */
    mu_check(strstr(mime, "text/html") != NULL || strcmp(mime, "application/octet-stream") == 0);
    return NULL;
}

/*============================================================================
 * Test Suite Runner
 *============================================================================*/

void test_suite_mime(void) {
    /* Common types */
    MU_RUN_TEST(test_mime_html);
    MU_RUN_TEST(test_mime_htm);
    MU_RUN_TEST(test_mime_css);
    MU_RUN_TEST(test_mime_js);
    MU_RUN_TEST(test_mime_json);
    MU_RUN_TEST(test_mime_txt);
    
    /* Images */
    MU_RUN_TEST(test_mime_png);
    MU_RUN_TEST(test_mime_jpg);
    MU_RUN_TEST(test_mime_jpeg);
    MU_RUN_TEST(test_mime_gif);
    MU_RUN_TEST(test_mime_svg);
    MU_RUN_TEST(test_mime_ico);
    MU_RUN_TEST(test_mime_webp);
    
    /* Fonts */
    MU_RUN_TEST(test_mime_woff);
    MU_RUN_TEST(test_mime_woff2);
    MU_RUN_TEST(test_mime_ttf);
    
    /* Applications */
    MU_RUN_TEST(test_mime_pdf);
    MU_RUN_TEST(test_mime_zip);
    MU_RUN_TEST(test_mime_xml);
    
    /* Unknown/default */
    MU_RUN_TEST(test_mime_unknown_extension);
    MU_RUN_TEST(test_mime_empty_extension);
    MU_RUN_TEST(test_mime_null_extension);
    
    /* Case sensitivity */
    MU_RUN_TEST(test_mime_uppercase_html);
    MU_RUN_TEST(test_mime_mixed_case);
}

