#ifndef CONFIG_H
#define CONFIG_H

#include "bolt.h"
#include <stdbool.h>

/*
 * Bolt server configuration structure.
 */
typedef struct BoltConfig {
    /* Server settings */
    int port;
    char bind_address[64];  /* Empty = INADDR_ANY */
    int worker_threads;         /* 0 = auto-detect */
    int max_connections;
    
    /* File serving */
    char web_root[512];
    char index_file[64];
    size_t max_file_size;
    
    /* Compression */
    bool gzip_enabled;
    int gzip_level;
    size_t gzip_min_size;
    
    /* Logging */
    char access_log_path[512];
    char error_log_path[512];
    int log_level;  /* 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG */
    
    /* Rate limiting */
    int max_connections_per_ip;
    
    /* Timeouts */
    DWORD keepalive_timeout_ms;
    DWORD request_timeout_ms;
    
    /* Features */
    bool enable_dir_listing;
    bool enable_file_cache;
    
    /* TLS/SSL */
    bool tls_enabled;
    char tls_cert_file[512];
    char tls_key_file[512];
} BoltConfig;

/*
 * Load configuration from file.
 * Returns true on success, false on failure.
 */
bool config_load_from_file(BoltConfig* config, const char* config_path);

/*
 * Load default configuration.
 */
void config_load_defaults(BoltConfig* config);

/*
 * Free configuration resources (strings).
 */
void config_free(BoltConfig* config);

/*
 * Validate configuration.
 * Returns true if valid, false otherwise.
 */
bool config_validate(const BoltConfig* config);

#endif /* CONFIG_H */

