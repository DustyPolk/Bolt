#include "../include/master.h"
#include "../include/bolt_server.h"
#include "../include/config.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_is_master = false;

/*
 * Worker process entry point.
 */
static DWORD WINAPI worker_process(LPVOID param) {
    char** argv = (char**)param;
    int argc = 0;
    while (argv[argc]) argc++;
    
    /* Load config */
    BoltConfig config;
    config_load_defaults(&config);
    
    /* Create and run server */
    BoltServer* server = bolt_server_create_with_config(&config);
    if (server) {
        bolt_server_run(server);
        bolt_server_destroy(server);
    }
    
    return 0;
}

/*
 * Run as master process (manages workers).
 */
bool master_run(int argc, char* argv[], int worker_count) {
    if (worker_count < 1) worker_count = 1;
    
    g_is_master = true;
    
    printf("Master process starting %d workers...\n", worker_count);
    
    HANDLE* workers = (HANDLE*)calloc(worker_count, sizeof(HANDLE));
    if (!workers) return false;
    
    /* Start worker processes */
    for (int i = 0; i < worker_count; i++) {
        HANDLE thread = CreateThread(NULL, 0, worker_process, argv, 0, NULL);
        if (thread) {
            workers[i] = thread;
            printf("Started worker %d\n", i + 1);
        }
    }
    
    /* Wait for all workers */
    WaitForMultipleObjects(worker_count, workers, TRUE, INFINITE);
    
    /* Cleanup */
    for (int i = 0; i < worker_count; i++) {
        if (workers[i]) {
            CloseHandle(workers[i]);
        }
    }
    free(workers);
    
    return true;
}

/*
 * Run as worker process.
 */
bool worker_run(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    g_is_master = false;
    
    /* Worker runs server directly */
    BoltConfig config;
    config_load_defaults(&config);
    
    BoltServer* server = bolt_server_create_with_config(&config);
    if (!server) return false;
    
    bolt_server_run(server);
    bolt_server_destroy(server);
    
    return true;
}

/*
 * Check if current process is master.
 */
bool master_is_master(void) {
    return g_is_master;
}

