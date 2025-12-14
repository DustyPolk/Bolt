#ifndef PROFILER_H
#define PROFILER_H

#include "bolt.h"
#include "logger.h"
#include "connection.h"
#include <stdbool.h>

/*
 * Request timing information.
 */
typedef struct BoltRequestTiming {
    ULONGLONG accept_time;
    ULONGLONG parse_time;
    ULONGLONG file_read_time;
    ULONGLONG send_time;
    ULONGLONG total_time;
} BoltRequestTiming;

/*
 * Initialize profiler.
 */
void profiler_init(DWORD slow_request_threshold_ms);

/*
 * Start timing a request.
 */
void profiler_start_request(BoltConnection* conn);

/*
 * Record timing milestone.
 */
void profiler_record_milestone(BoltConnection* conn, const char* milestone);

/*
 * End timing and log if slow.
 */
void profiler_end_request(BoltConnection* conn, BoltLogger* logger);

/*
 * Get memory usage statistics.
 */
void profiler_get_memory_stats(size_t* total_memory, size_t* used_memory);

#endif /* PROFILER_H */

