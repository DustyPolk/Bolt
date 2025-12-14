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
    printf("  Usage: %s [port] [--stats] [--stats-interval-ms N]\n", program);
    printf("\n");
    printf("  Arguments:\n");
    printf("    port    Port number to listen on (default: %d)\n", BOLT_DEFAULT_PORT);
    printf("    --stats Print periodic stats to console\n");
    printf("    --stats-interval-ms N  Stats print interval (default: 1000)\n");
    printf("\n");
    printf("  Example:\n");
    printf("    %s 8080\n", program);
    printf("\n");
}

/*
 * Main entry point.
 */
int main(int argc, char* argv[]) {
    int port = BOLT_DEFAULT_PORT;
    bool stats = false;
    DWORD stats_interval_ms = 1000;
    
    /* Parse command line */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
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
        /* If it's not a flag, treat it as port */
        if (argv[i][0] != '-') {
            port = atoi(argv[i]);
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "Invalid port number: %s\n", argv[i]);
                return 1;
            }
            continue;
        }
        fprintf(stderr, "Unknown argument: %s\n", argv[i]);
        print_usage(argv[0]);
        return 1;
    }
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create server */
    g_server = bolt_server_create(port);
    if (!g_server) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }

    g_server->stats_enabled = stats;
    g_server->stats_interval_ms = stats_interval_ms;
    
    /* Run server (blocks until stopped) */
    bolt_server_run(g_server);
    
    /* Cleanup */
    bolt_server_destroy(g_server);
    g_server = NULL;
    
    return 0;
}
