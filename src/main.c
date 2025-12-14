/*
 * ⚡ BOLT - High-Performance HTTP Server
 * 
 * A blazingly fast static file server featuring:
 * - Windows IOCP for async I/O (10,000+ concurrent connections)
 * - Thread pool for parallel request handling
 * - TransmitFile for zero-copy file transfers
 * - HTTP Keep-Alive for connection reuse
 * - Memory pooling for allocation efficiency
 */

#include "../include/bolt.h"
#include "../include/bolt_server.h"
#include "../include/config.h"
#include "../include/service.h"
#include "../include/reload.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

/* Global server for signal handling */
static BoltServer* g_server = NULL;

/*
 * Signal handler for graceful shutdown.
 */
static void signal_handler(int sig) {
    (void)sig;
    if (g_server) {
        bolt_server_stop(g_server);
    }
}

/*
 * Print usage information.
 */
static void print_usage(const char* program) {
    printf("\n");
    printf("  ⚡ BOLT - High-Performance HTTP Server\n");
    printf("  ========================================\n");
    printf("\n");
    printf("  Usage: %s [port] [options]\n", program);
    printf("\n");
    printf("  Arguments:\n");
    printf("    port              Port number to listen on (default: %d)\n", BOLT_DEFAULT_PORT);
    printf("    -c, --config FILE Configuration file path (default: bolt.conf)\n");
    printf("    --stats           Print periodic stats to console\n");
    printf("    --stats-interval-ms N  Stats print interval (default: 1000)\n");
    printf("    -d, --daemon      Run as Windows Service\n");
    printf("    --install-service Install as Windows Service\n");
    printf("    --uninstall-service Uninstall Windows Service\n");
    printf("\n");
    printf("  Example:\n");
    printf("    %s 8080\n", program);
    printf("    %s -c /path/to/bolt.conf\n", program);
    printf("\n");
}

/*
 * Main entry point.
 */
int main(int argc, char* argv[]) {
    BoltConfig config;
    bool stats = false;
    DWORD stats_interval_ms = 1000;
    const char* config_path = "bolt.conf";
    
    /* Load default config */
    config_load_defaults(&config);
    
    /* Check for service operations */
    if (argc > 1) {
        if (strcmp(argv[1], "--install-service") == 0) {
            bool success = service_install("BoltServer", "Bolt HTTP Server",
                                          "High-performance HTTP static file server",
                                          argc > 2 ? argv[2] : NULL);
            if (success) {
                printf("Service installed successfully.\n");
            } else {
                fprintf(stderr, "Failed to install service.\n");
            }
            return success ? 0 : 1;
        }
        
        if (strcmp(argv[1], "--uninstall-service") == 0) {
            bool success = service_uninstall("BoltServer");
            if (success) {
                printf("Service uninstalled successfully.\n");
            } else {
                fprintf(stderr, "Failed to uninstall service.\n");
            }
            return success ? 0 : 1;
        }
        
        if (strcmp(argv[1], "-d") == 0 || strcmp(argv[1], "--daemon") == 0) {
            return service_run("BoltServer", argc - 1, argv + 1) ? 0 : 1;
        }
    }
    
    /* Parse command line */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                config_path = argv[i + 1];
                i++;
            }
            continue;
        }
        if (strcmp(argv[i], "--stats") == 0) {
            stats = true;
            continue;
        }
        if (strcmp(argv[i], "--stats-interval-ms") == 0 && i + 1 < argc) {
            stats_interval_ms = (DWORD)atoi(argv[i + 1]);
            i++;
            continue;
        }
        /* If it's not a flag, treat it as port (override config) */
        if (argv[i][0] != '-') {
            int port = atoi(argv[i]);
            if (port > 0 && port <= 65535) {
                config.port = port;
            } else {
                fprintf(stderr, "Invalid port number: %s\n", argv[i]);
                return 1;
            }
            continue;
        }
        fprintf(stderr, "Unknown argument: %s\n", argv[i]);
        print_usage(argv[0]);
        return 1;
    }
    
    /* Load config file (if exists) */
    if (!config_load_from_file(&config, config_path)) {
        fprintf(stderr, "Failed to load config from %s, using defaults\n", config_path);
    }
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create server with config */
    g_server = bolt_server_create_with_config(&config);
    if (!g_server) {
        fprintf(stderr, "Failed to create server\n");
        config_free(&config);
        return 1;
    }

    g_server->stats_enabled = stats;
    g_server->stats_interval_ms = stats_interval_ms;
    
    /* Setup config reload handler */
    reload_setup_signal_handler(g_server);
    
    /* Run server (blocks until stopped) */
    bolt_server_run(g_server);
    
    /* Cleanup */
    bolt_server_destroy(g_server);
    g_server = NULL;
    config_free(&config);
    
    return 0;
}
