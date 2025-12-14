#include "../include/logger.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

/*
 * Format HTTP date for logs (Common Log Format uses local time).
 */
void logger_format_date(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    /* Format: [02/Jan/2024:15:04:05 -0500] */
    strftime(buffer, buffer_size, "[%d/%b/%Y:%H:%M:%S %z]", tm_info);
}

/*
 * Create logger.
 */
BoltLogger* logger_create(const char* access_log_path, const char* error_log_path, BoltLogLevel level) {
    BoltLogger* logger = (BoltLogger*)calloc(1, sizeof(BoltLogger));
    if (!logger) return NULL;
    
    logger->level = level;
    logger->enabled = true;
    InitializeCriticalSection(&logger->lock);
    
    /* Open access log */
    if (access_log_path && access_log_path[0]) {
        /* Create directory if needed */
        char dir[512];
        strncpy(dir, access_log_path, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        
        /* Find last slash */
        char* last_slash = strrchr(dir, '/');
        if (!last_slash) last_slash = strrchr(dir, '\\');
        if (last_slash) {
            *last_slash = '\0';
            /* Try to create directory (Windows) */
            CreateDirectoryA(dir, NULL);
        }
        
        logger->access_log = fopen(access_log_path, "a");
        if (!logger->access_log) {
            fprintf(stderr, "Warning: Failed to open access log: %s\n", access_log_path);
        }
    }
    
    /* Open error log */
    if (error_log_path && error_log_path[0]) {
        /* Create directory if needed */
        char dir[512];
        strncpy(dir, error_log_path, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        
        char* last_slash = strrchr(dir, '/');
        if (!last_slash) last_slash = strrchr(dir, '\\');
        if (last_slash) {
            *last_slash = '\0';
            CreateDirectoryA(dir, NULL);
        }
        
        logger->error_log = fopen(error_log_path, "a");
        if (!logger->error_log) {
            fprintf(stderr, "Warning: Failed to open error log: %s\n", error_log_path);
        }
    }
    
    return logger;
}

/*
 * Destroy logger.
 */
void logger_destroy(BoltLogger* logger) {
    if (!logger) return;
    
    EnterCriticalSection(&logger->lock);
    
    if (logger->access_log && logger->access_log != stdout && logger->access_log != stderr) {
        fclose(logger->access_log);
    }
    
    if (logger->error_log && logger->error_log != stdout && logger->error_log != stderr) {
        fclose(logger->error_log);
    }
    
    LeaveCriticalSection(&logger->lock);
    DeleteCriticalSection(&logger->lock);
    free(logger);
}

/*
 * Log an access entry (Common Log Format + Combined Log Format).
 * Format: %h %l %u %t "%r" %>s %b "%{Referer}i" "%{User-Agent}i"
 */
void logger_access(BoltLogger* logger, const char* client_ip, const char* method,
                   const char* uri, int status, size_t bytes_sent, const char* referer,
                   const char* user_agent) {
    if (!logger || !logger->enabled || !logger->access_log) return;
    
    EnterCriticalSection(&logger->lock);
    
    char date[64];
    logger_format_date(date, sizeof(date));
    
    /* Common Log Format + Combined Log Format */
    fprintf(logger->access_log,
            "%s - - %s \"%s %s HTTP/1.1\" %d %zu \"%s\" \"%s\"\n",
            client_ip ? client_ip : "-",
            date,
            method ? method : "-",
            uri ? uri : "-",
            status,
            bytes_sent,
            referer && referer[0] ? referer : "-",
            user_agent && user_agent[0] ? user_agent : "-");
    
    fflush(logger->access_log);
    
    LeaveCriticalSection(&logger->lock);
}

/*
 * Log an error message.
 */
void logger_error(BoltLogger* logger, BoltLogLevel level, const char* format, ...) {
    if (!logger || !logger->enabled) return;
    if (level > logger->level) return;  /* Filter by log level */
    
    FILE* log_file = logger->error_log ? logger->error_log : stderr;
    
    EnterCriticalSection(&logger->lock);
    
    char date[64];
    logger_format_date(date, sizeof(date));
    
    const char* level_str = "UNKNOWN";
    switch (level) {
        case BOLT_LOG_ERROR: level_str = "ERROR"; break;
        case BOLT_LOG_WARN: level_str = "WARN"; break;
        case BOLT_LOG_INFO: level_str = "INFO"; break;
        case BOLT_LOG_DEBUG: level_str = "DEBUG"; break;
    }
    
    fprintf(log_file, "%s [%s] ", date, level_str);
    
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
    
    LeaveCriticalSection(&logger->lock);
}

