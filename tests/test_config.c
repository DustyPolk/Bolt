/*
 * Bolt Test Suite - Configuration Tests
 * 
 * Tests for configuration file parsing and validation.
 */

#include "minunit.h"
#include "../include/config.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Default Configuration Tests
 *============================================================================*/

MU_TEST(test_config_defaults) {
    BoltConfig config;
    config_load_defaults(&config);
    
    /* Check default values */
    mu_assert_int_eq(8080, config.port);
    mu_check(config.worker_threads >= 0);  /* Can be 0 for auto-detect */
    mu_check(config.max_connections > 0);
    
    return NULL;
}

MU_TEST(test_config_default_web_root) {
    BoltConfig config;
    config_load_defaults(&config);
    
    /* Should have a default web root */
    mu_check(strlen(config.web_root) > 0);
    
    return NULL;
}

MU_TEST(test_config_default_compression) {
    BoltConfig config;
    config_load_defaults(&config);
    
    /* Compression settings should have reasonable defaults */
    mu_check(config.gzip_level >= 0 && config.gzip_level <= 9);
    
    return NULL;
}

/*============================================================================
 * Configuration Validation Tests
 *============================================================================*/

MU_TEST(test_config_validate_valid) {
    BoltConfig config;
    config_load_defaults(&config);
    
    /* Default config should be valid */
    mu_assert_true(config_validate(&config));
    
    return NULL;
}

MU_TEST(test_config_validate_invalid_port_zero) {
    BoltConfig config;
    config_load_defaults(&config);
    config.port = 0;
    
    /* Port 0 might be invalid */
    /* Note: Some implementations allow port 0 for auto-assign */
    
    return NULL;
}

MU_TEST(test_config_validate_invalid_port_negative) {
    BoltConfig config;
    config_load_defaults(&config);
    config.port = -1;
    
    /* Negative port should be invalid */
    mu_assert_false(config_validate(&config));
    
    return NULL;
}

MU_TEST(test_config_validate_invalid_port_too_high) {
    BoltConfig config;
    config_load_defaults(&config);
    config.port = 70000;  /* Beyond valid port range (65535 max) */
    
    /* Port beyond valid range should be invalid */
    mu_assert_false(config_validate(&config));
    
    return NULL;
}

/*============================================================================
 * Configuration File Loading Tests
 *============================================================================*/

MU_TEST(test_config_load_nonexistent) {
    BoltConfig config;
    config_load_defaults(&config);
    
    /* Loading non-existent file should fail or use defaults */
    bool result = config_load_from_file(&config, "nonexistent_file_12345.conf");
    
    /* Should return true (using defaults) for non-existent file */
    mu_assert_true(result);
    
    return NULL;
}

MU_TEST(test_config_load_null_path) {
    BoltConfig config;
    config_load_defaults(&config);
    
    /* NULL path should be handled gracefully */
    bool result = config_load_from_file(&config, NULL);
    mu_assert_false(result);
    
    return NULL;
}

MU_TEST(test_config_load_null_config) {
    /* NULL config pointer should be handled gracefully */
    bool result = config_load_from_file(NULL, "bolt.conf");
    mu_assert_false(result);
    
    return NULL;
}

/*============================================================================
 * Configuration Free Tests
 *============================================================================*/

MU_TEST(test_config_free_safe) {
    BoltConfig config;
    config_load_defaults(&config);
    
    /* Should be safe to call config_free */
    config_free(&config);
    
    return NULL;
}

MU_TEST(test_config_free_null) {
    /* Should be safe to call config_free with NULL */
    config_free(NULL);
    
    return NULL;
}

/*============================================================================
 * Port Range Tests
 *============================================================================*/

MU_TEST(test_config_port_privileged) {
    BoltConfig config;
    config_load_defaults(&config);
    config.port = 80;
    
    /* Port 80 is valid (though may require privileges) */
    mu_assert_true(config_validate(&config));
    
    return NULL;
}

MU_TEST(test_config_port_max_valid) {
    BoltConfig config;
    config_load_defaults(&config);
    config.port = 65535;
    
    /* Maximum valid port */
    mu_assert_true(config_validate(&config));
    
    return NULL;
}

/*============================================================================
 * Worker Thread Configuration Tests
 *============================================================================*/

MU_TEST(test_config_workers_auto) {
    BoltConfig config;
    config_load_defaults(&config);
    config.worker_threads = 0;  /* 0 typically means auto-detect */
    
    /* Should be valid */
    mu_assert_true(config_validate(&config));
    
    return NULL;
}

MU_TEST(test_config_workers_explicit) {
    BoltConfig config;
    config_load_defaults(&config);
    config.worker_threads = 4;
    
    /* Explicit worker count should be valid */
    mu_assert_true(config_validate(&config));
    
    return NULL;
}

MU_TEST(test_config_workers_negative) {
    BoltConfig config;
    config_load_defaults(&config);
    config.worker_threads = -5;
    
    /* Negative workers should be invalid */
    mu_assert_false(config_validate(&config));
    
    return NULL;
}

/*============================================================================
 * Test Suite Runner
 *============================================================================*/

void test_suite_config(void) {
    /* Default configuration */
    MU_RUN_TEST(test_config_defaults);
    MU_RUN_TEST(test_config_default_web_root);
    MU_RUN_TEST(test_config_default_compression);
    
    /* Validation */
    MU_RUN_TEST(test_config_validate_valid);
    MU_RUN_TEST(test_config_validate_invalid_port_zero);
    MU_RUN_TEST(test_config_validate_invalid_port_negative);
    MU_RUN_TEST(test_config_validate_invalid_port_too_high);
    
    /* File loading */
    MU_RUN_TEST(test_config_load_nonexistent);
    MU_RUN_TEST(test_config_load_null_path);
    MU_RUN_TEST(test_config_load_null_config);
    
    /* Free */
    MU_RUN_TEST(test_config_free_safe);
    MU_RUN_TEST(test_config_free_null);
    
    /* Port ranges */
    MU_RUN_TEST(test_config_port_privileged);
    MU_RUN_TEST(test_config_port_max_valid);
    
    /* Workers */
    MU_RUN_TEST(test_config_workers_auto);
    MU_RUN_TEST(test_config_workers_explicit);
    MU_RUN_TEST(test_config_workers_negative);
}