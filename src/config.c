#include "../include/config.h"
#include "../include/bolt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Default configuration values */
static const char* DEFAULT_WEB_ROOT = "public";
static const char* DEFAULT_INDEX_FILE = "index.html";
static const char* DEFAULT_ACCESS_LOG = "./logs/access.log";
static const char* DEFAULT_ERROR_LOG = "./logs/error.log";

/*
 * Trim whitespace from string.
 */
static void trim_string(char* str) {
    if (!str) return;
    
    /* Trim leading whitespace */
    char* start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    
    /* Trim trailing whitespace */
    char* end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    
    /* Move trimmed string to start */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/*
 * Parse a line from config file.
 */
static bool parse_config_line(BoltConfig* config, const char* line) {
    if (!line || !config) return false;
    
    /* Skip comments and empty lines */
    while (*line && isspace((unsigned char)*line)) line++;
    if (*line == '\0' || *line == '#' || *line == ';') {
        return true;  /* Skip but don't error */
    }
    
    /* Find '=' or end of directive */
    const char* eq = strchr(line, '=');
    const char* semicolon = strchr(line, ';');
    
    if (!eq) {
        /* No '=' - might be a block start/end or directive without value */
        char directive[256];
        sscanf(line, "%255s", directive);
        trim_string(directive);
        
        /* Handle block directives (for future: server {}, location {}, etc.) */
        if (strcmp(directive, "server") == 0 || strcmp(directive, "{") == 0 ||
            strcmp(directive, "}") == 0) {
            return true;  /* Ignore for now - Phase 2 feature */
        }
        
        return true;  /* Unknown directive, skip */
    }
    
    /* Extract key and value */
    char key[256];
    char value[512];
    
    size_t key_len = eq - line;
    if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
    strncpy(key, line, key_len);
    key[key_len] = '\0';
    trim_string(key);
    
    const char* value_start = eq + 1;
    size_t value_len = 0;
    if (semicolon && semicolon > value_start) {
        value_len = semicolon - value_start;
    } else {
        value_len = strlen(value_start);
    }
    if (value_len >= sizeof(value)) value_len = sizeof(value) - 1;
    strncpy(value, value_start, value_len);
    value[value_len] = '\0';
    trim_string(value);
    
    /* Parse configuration directives */
    if (strcmp(key, "listen") == 0 || strcmp(key, "port") == 0) {
        config->port = atoi(value);
        if (config->port <= 0 || config->port > 65535) {
            config->port = BOLT_DEFAULT_PORT;
        }
    } else if (strcmp(key, "root") == 0 || strcmp(key, "web_root") == 0) {
        if (strlen(value) > 0) {
            strncpy(config->web_root, value, sizeof(config->web_root) - 1);
            config->web_root[sizeof(config->web_root) - 1] = '\0';
        }
    } else if (strcmp(key, "worker_threads") == 0 || strcmp(key, "workers") == 0) {
        if (strcmp(value, "auto") == 0) {
            config->worker_threads = 0;
        } else {
            config->worker_threads = atoi(value);
        }
    } else if (strcmp(key, "max_connections") == 0) {
        config->max_connections = atoi(value);
    } else if (strcmp(key, "index") == 0 || strcmp(key, "index_file") == 0) {
        strncpy(config->index_file, value, sizeof(config->index_file) - 1);
        config->index_file[sizeof(config->index_file) - 1] = '\0';
    } else if (strcmp(key, "gzip") == 0 || strcmp(key, "gzip_enabled") == 0) {
        config->gzip_enabled = (strcmp(value, "on") == 0 || strcmp(value, "1") == 0 ||
                                strcmp(value, "yes") == 0 || strcmp(value, "true") == 0);
    } else if (strcmp(key, "gzip_level") == 0) {
        config->gzip_level = atoi(value);
        if (config->gzip_level < 1) config->gzip_level = 1;
        if (config->gzip_level > 9) config->gzip_level = 9;
    } else if (strcmp(key, "gzip_min_size") == 0) {
        config->gzip_min_size = (size_t)atoi(value);
    } else if (strcmp(key, "access_log") == 0) {
        strncpy(config->access_log_path, value, sizeof(config->access_log_path) - 1);
        config->access_log_path[sizeof(config->access_log_path) - 1] = '\0';
    } else if (strcmp(key, "error_log") == 0) {
        strncpy(config->error_log_path, value, sizeof(config->error_log_path) - 1);
        config->error_log_path[sizeof(config->error_log_path) - 1] = '\0';
    } else if (strcmp(key, "log_level") == 0) {
        if (strcmp(value, "error") == 0) config->log_level = 0;
        else if (strcmp(value, "warn") == 0) config->log_level = 1;
        else if (strcmp(value, "info") == 0) config->log_level = 2;
        else if (strcmp(value, "debug") == 0) config->log_level = 3;
    } else if (strcmp(key, "rate_limit_per_ip") == 0) {
        config->max_connections_per_ip = atoi(value);
    } else if (strcmp(key, "keepalive_timeout") == 0) {
        config->keepalive_timeout_ms = (DWORD)atoi(value) * 1000;
    } else if (strcmp(key, "request_timeout") == 0) {
        config->request_timeout_ms = (DWORD)atoi(value) * 1000;
    } else if (strcmp(key, "enable_dir_listing") == 0) {
        config->enable_dir_listing = (strcmp(value, "on") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "enable_file_cache") == 0) {
        config->enable_file_cache = (strcmp(value, "on") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "ssl") == 0 || strcmp(key, "tls") == 0 || strcmp(key, "tls_enabled") == 0) {
        config->tls_enabled = (strcmp(value, "on") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "ssl_certificate") == 0 || strcmp(key, "tls_certificate") == 0) {
        strncpy(config->tls_cert_file, value, sizeof(config->tls_cert_file) - 1);
        config->tls_cert_file[sizeof(config->tls_cert_file) - 1] = '\0';
    } else if (strcmp(key, "ssl_certificate_key") == 0 || strcmp(key, "tls_certificate_key") == 0) {
        strncpy(config->tls_key_file, value, sizeof(config->tls_key_file) - 1);
        config->tls_key_file[sizeof(config->tls_key_file) - 1] = '\0';
    }
    
    return true;
}

/*
 * Load default configuration.
 */
void config_load_defaults(BoltConfig* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    config->port = BOLT_DEFAULT_PORT;
    config->bind_address[0] = '\0';  /* Empty = INADDR_ANY */
    config->worker_threads = 0;  /* Auto-detect */
    config->max_connections = BOLT_MAX_CONNECTIONS;
    
    strncpy(config->web_root, DEFAULT_WEB_ROOT, sizeof(config->web_root) - 1);
    strncpy(config->index_file, DEFAULT_INDEX_FILE, sizeof(config->index_file) - 1);
    config->max_file_size = BOLT_MAX_FILE_SIZE;
    
    config->gzip_enabled = true;
    config->gzip_level = 6;
    config->gzip_min_size = 256;
    
    strncpy(config->access_log_path, DEFAULT_ACCESS_LOG, sizeof(config->access_log_path) - 1);
    strncpy(config->error_log_path, DEFAULT_ERROR_LOG, sizeof(config->error_log_path) - 1);
    config->log_level = 2;  /* INFO */
    
    config->max_connections_per_ip = BOLT_MAX_CONNECTIONS_PER_IP;
    
    config->keepalive_timeout_ms = BOLT_KEEPALIVE_TIMEOUT;
    config->request_timeout_ms = BOLT_REQUEST_TIMEOUT;
    
    config->enable_dir_listing = false;
    config->enable_file_cache = true;
    
    config->tls_enabled = false;
    config->tls_cert_file[0] = '\0';
    config->tls_key_file[0] = '\0';
}

/*
 * Load configuration from file.
 */
bool config_load_from_file(BoltConfig* config, const char* config_path) {
    if (!config || !config_path) return false;
    
    /* Load defaults first */
    config_load_defaults(config);
    
    FILE* f = fopen(config_path, "r");
    if (!f) {
        /* Config file not found - use defaults */
        return true;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        if (len > 1 && line[len - 2] == '\r') {
            line[len - 2] = '\0';
        }
        
        parse_config_line(config, line);
    }
    
    fclose(f);
    
    return config_validate(config);
}

/*
 * Validate configuration.
 */
bool config_validate(const BoltConfig* config) {
    if (!config) return false;
    
    if (config->port < 1 || config->port > 65535) {
        return false;
    }
    
    if (config->max_connections < 1) {
        return false;
    }
    
    if (config->gzip_level < 1 || config->gzip_level > 9) {
        return false;
    }
    
    return true;
}

/*
 * Free configuration resources.
 */
void config_free(BoltConfig* config) {
    /* For now, strings point to static/default values or file buffer */
    /* Phase 2: implement dynamic allocation and free here */
    (void)config;
}

