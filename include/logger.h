#ifndef LOGGER_H
#define LOGGER_H

#include "bolt.h"
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

/*
 * Log levels.
 */
typedef enum {
    BOLT_LOG_ERROR = 0,
    BOLT_LOG_WARN = 1,
    BOLT_LOG_INFO = 2,
    BOLT_LOG_DEBUG = 3
} BoltLogLevel;

/*
 * Logger structure.
 */
typedef struct BoltLogger {
    FILE* access_log;
    FILE* error_log;
    BoltLogLevel level;
    bool enabled;
    CRITICAL_SECTION lock;  /* Thread-safe logging */
} BoltLogger;

/*
 * Initialize logger.
 */
BoltLogger* logger_create(const char* access_log_path, const char* error_log_path, BoltLogLevel level);

/*
 * Destroy logger.
 */
void logger_destroy(BoltLogger* logger);

/*
 * Log an access entry (Common Log Format).
 */
void logger_access(BoltLogger* logger, const char* client_ip, const char* method,
                   const char* uri, int status, size_t bytes_sent, const char* referer,
                   const char* user_agent);

/*
 * Log an error message.
 */
void logger_error(BoltLogger* logger, BoltLogLevel level, const char* format, ...);

/*
 * Format HTTP date for logs.
 */
void logger_format_date(char* buffer, size_t buffer_size);

#endif /* LOGGER_H */

