#include "../include/profiler.h"
#include "../include/connection.h"
#include "../include/logger.h"
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>

static DWORD g_slow_threshold_ms = 1000;

/*
 * Initialize profiler.
 */
void profiler_init(DWORD slow_request_threshold_ms) {
    g_slow_threshold_ms = slow_request_threshold_ms;
}

/*
 * Start timing a request.
 */
void profiler_start_request(BoltConnection* conn) {
    if (!conn) return;
    conn->connect_time = GetTickCount64();
}

/*
 * Record timing milestone.
 */
void profiler_record_milestone(BoltConnection* conn, const char* milestone) {
    (void)conn;
    (void)milestone;
    /* TODO: Store milestone timings */
}

/*
 * End timing and log if slow.
 */
void profiler_end_request(BoltConnection* conn, BoltLogger* logger) {
    if (!conn || !logger) return;
    
    ULONGLONG now = GetTickCount64();
    ULONGLONG total_time = now - conn->connect_time;
    
    if (total_time > g_slow_threshold_ms) {
        char ip_str[64] = "unknown";
        struct in_addr addr;
        addr.s_addr = conn->client_ip;
        inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
        
        logger_error(logger, BOLT_LOG_WARN,
                    "Slow request: %llu ms from %s for %s",
                    total_time, ip_str, conn->request.uri);
    }
}

/*
 * Get memory usage statistics.
 */
void profiler_get_memory_stats(size_t* total_memory, size_t* used_memory) {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        if (total_memory) {
            *total_memory = pmc.WorkingSetSize;
        }
        if (used_memory) {
            *used_memory = pmc.PrivateUsage;
        }
    } else {
        if (total_memory) *total_memory = 0;
        if (used_memory) *used_memory = 0;
    }
}

