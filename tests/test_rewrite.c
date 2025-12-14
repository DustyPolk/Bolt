/*
 * Bolt Test Suite - URL Rewrite Tests
 * 
 * Tests for URL rewriting and pattern matching.
 */

#include "minunit.h"
#include "../include/rewrite.h"
#include <string.h>

/*============================================================================
 * Pattern Matching Tests
 *============================================================================*/

MU_TEST(test_match_exact) {
    mu_assert_true(rewrite_match_pattern("/index.html", "/index.html"));
    mu_assert_false(rewrite_match_pattern("/index.html", "/about.html"));
    return NULL;
}

MU_TEST(test_match_wildcard_end) {
    /* /api/* should match anything starting with /api/ */
    mu_assert_true(rewrite_match_pattern("/api/*", "/api/users"));
    mu_assert_true(rewrite_match_pattern("/api/*", "/api/users/123"));
    mu_assert_true(rewrite_match_pattern("/api/*", "/api/"));
    mu_assert_false(rewrite_match_pattern("/api/*", "/api"));
    return NULL;
}

MU_TEST(test_match_wildcard_middle) {
    /* /user/*/profile should match user profiles */
    mu_assert_true(rewrite_match_pattern("/user/*/profile", "/user/123/profile"));
    mu_assert_true(rewrite_match_pattern("/user/*/profile", "/user/john/profile"));
    mu_assert_false(rewrite_match_pattern("/user/*/profile", "/user/123/settings"));
    return NULL;
}

MU_TEST(test_match_wildcard_start) {
    /* *.html should match HTML files */
    mu_assert_true(rewrite_match_pattern("*.html", "index.html"));
    mu_assert_true(rewrite_match_pattern("*.html", "about.html"));
    mu_assert_false(rewrite_match_pattern("*.html", "style.css"));
    return NULL;
}

MU_TEST(test_match_multiple_wildcards) {
    /* /*/files/* should match nested paths */
    mu_assert_true(rewrite_match_pattern("/*/files/*", "/user/files/doc.pdf"));
    mu_assert_true(rewrite_match_pattern("/*/files/*", "/admin/files/report.csv"));
    return NULL;
}

MU_TEST(test_match_question_mark) {
    /* ? matches single character */
    mu_assert_true(rewrite_match_pattern("/file?.txt", "/file1.txt"));
    mu_assert_true(rewrite_match_pattern("/file?.txt", "/filea.txt"));
    mu_assert_false(rewrite_match_pattern("/file?.txt", "/file12.txt"));
    return NULL;
}

/*============================================================================
 * Rewrite Engine Tests
 *============================================================================*/

MU_TEST(test_engine_create_destroy) {
    BoltRewriteEngine* engine = rewrite_engine_create();
    mu_assert_not_null(engine);
    rewrite_engine_destroy(engine);
    return NULL;
}

MU_TEST(test_engine_add_rule) {
    BoltRewriteEngine* engine = rewrite_engine_create();
    mu_assert_not_null(engine);
    
    bool result = rewrite_add_rule(engine, "/old/*", "/new/$1", REWRITE_INTERNAL);
    mu_assert_true(result);
    
    rewrite_engine_destroy(engine);
    return NULL;
}

MU_TEST(test_engine_apply_simple) {
    BoltRewriteEngine* engine = rewrite_engine_create();
    mu_assert_not_null(engine);
    
    rewrite_add_rule(engine, "/old", "/new", REWRITE_INTERNAL);
    
    char out_uri[2048];
    bool rewritten = rewrite_apply(engine, "/old", out_uri, sizeof(out_uri));
    
    mu_assert_true(rewritten);
    mu_assert_string_eq("/new", out_uri);
    
    rewrite_engine_destroy(engine);
    return NULL;
}

MU_TEST(test_engine_no_match) {
    BoltRewriteEngine* engine = rewrite_engine_create();
    mu_assert_not_null(engine);
    
    rewrite_add_rule(engine, "/old", "/new", REWRITE_INTERNAL);
    
    char out_uri[2048];
    bool rewritten = rewrite_apply(engine, "/other", out_uri, sizeof(out_uri));
    
    mu_assert_false(rewritten);
    
    rewrite_engine_destroy(engine);
    return NULL;
}

MU_TEST(test_engine_null_inputs) {
    char out_uri[2048];
    
    /* NULL engine */
    mu_assert_false(rewrite_apply(NULL, "/test", out_uri, sizeof(out_uri)));
    
    BoltRewriteEngine* engine = rewrite_engine_create();
    
    /* NULL URI */
    mu_assert_false(rewrite_apply(engine, NULL, out_uri, sizeof(out_uri)));
    
    /* NULL output */
    mu_assert_false(rewrite_apply(engine, "/test", NULL, sizeof(out_uri)));
    
    /* Zero size */
    mu_assert_false(rewrite_apply(engine, "/test", out_uri, 0));
    
    rewrite_engine_destroy(engine);
    return NULL;
}

/*============================================================================
 * Redirect Type Tests
 *============================================================================*/

MU_TEST(test_rewrite_type_internal) {
    BoltRewriteEngine* engine = rewrite_engine_create();
    rewrite_add_rule(engine, "/blog/*", "/posts/$1", REWRITE_INTERNAL);
    
    char out_uri[2048];
    bool rewritten = rewrite_apply(engine, "/blog/hello", out_uri, sizeof(out_uri));
    
    mu_assert_true(rewritten);
    
    rewrite_engine_destroy(engine);
    return NULL;
}

MU_TEST(test_rewrite_type_301) {
    BoltRewriteEngine* engine = rewrite_engine_create();
    
    /* Add a 301 permanent redirect rule */
    bool result = rewrite_add_rule(engine, "/oldpage", "/newpage", REWRITE_REDIRECT_301);
    mu_assert_true(result);
    
    rewrite_engine_destroy(engine);
    return NULL;
}

MU_TEST(test_rewrite_type_302) {
    BoltRewriteEngine* engine = rewrite_engine_create();
    
    /* Add a 302 temporary redirect rule */
    bool result = rewrite_add_rule(engine, "/temp", "/target", REWRITE_REDIRECT_302);
    mu_assert_true(result);
    
    rewrite_engine_destroy(engine);
    return NULL;
}

/*============================================================================
 * Edge Case Tests
 *============================================================================*/

MU_TEST(test_rewrite_empty_uri) {
    BoltRewriteEngine* engine = rewrite_engine_create();
    rewrite_add_rule(engine, "", "/default", REWRITE_INTERNAL);
    
    char out_uri[2048];
    bool rewritten = rewrite_apply(engine, "", out_uri, sizeof(out_uri));
    
    /* Empty URI handling depends on implementation */
    
    rewrite_engine_destroy(engine);
    return NULL;
}

MU_TEST(test_rewrite_very_long_uri) {
    BoltRewriteEngine* engine = rewrite_engine_create();
    rewrite_add_rule(engine, "/long/*", "/short", REWRITE_INTERNAL);
    
    /* Create a very long URI */
    char long_uri[4096];
    memset(long_uri, 'a', sizeof(long_uri) - 1);
    long_uri[0] = '/';
    long_uri[sizeof(long_uri) - 1] = '\0';
    
    char out_uri[2048];
    /* Should handle gracefully without crashing */
    rewrite_apply(engine, long_uri, out_uri, sizeof(out_uri));
    
    rewrite_engine_destroy(engine);
    return NULL;
}

/*============================================================================
 * Test Suite Runner
 *============================================================================*/

void test_suite_rewrite(void) {
    /* Pattern matching */
    MU_RUN_TEST(test_match_exact);
    MU_RUN_TEST(test_match_wildcard_end);
    MU_RUN_TEST(test_match_wildcard_middle);
    MU_RUN_TEST(test_match_wildcard_start);
    MU_RUN_TEST(test_match_multiple_wildcards);
    MU_RUN_TEST(test_match_question_mark);
    
    /* Engine operations */
    MU_RUN_TEST(test_engine_create_destroy);
    MU_RUN_TEST(test_engine_add_rule);
    MU_RUN_TEST(test_engine_apply_simple);
    MU_RUN_TEST(test_engine_no_match);
    MU_RUN_TEST(test_engine_null_inputs);
    
    /* Redirect types */
    MU_RUN_TEST(test_rewrite_type_internal);
    MU_RUN_TEST(test_rewrite_type_301);
    MU_RUN_TEST(test_rewrite_type_302);
    
    /* Edge cases */
    MU_RUN_TEST(test_rewrite_empty_uri);
    MU_RUN_TEST(test_rewrite_very_long_uri);
}

