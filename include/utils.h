#ifndef UTILS_H
#define UTILS_H

#include "bolt.h"
#include <sys/stat.h>

/*
 * Utility functions for path handling and file operations.
 */

/* File information structure */
typedef struct {
    bool exists;
    bool is_directory;
    size_t size;
    time_t mtime;
} FileInfo;

/*
 * Sanitize a URL path to prevent directory traversal attacks.
 * Returns true if path is safe, false otherwise.
 * Sanitized path is written to out_path (must be at least MAX_PATH_LENGTH).
 */
bool utils_sanitize_path(const char* uri, char* out_path, size_t out_size);

/*
 * Get file information (exists, is_directory, size, mtime).
 */
FileInfo utils_get_file_info(const char* path);

/*
 * Get the file extension from a path.
 * Returns pointer to extension (without dot) or empty string if none.
 */
const char* utils_get_extension(const char* path);

/*
 * Format a file size as human-readable string (e.g., "1.5 KB").
 * Buffer should be at least 32 bytes.
 */
void utils_format_size(size_t size, char* buffer, size_t buffer_size);

/*
 * Format a timestamp as HTTP date (RFC 7231).
 * Buffer should be at least 64 bytes.
 */
void utils_format_http_date(time_t timestamp, char* buffer, size_t buffer_size);

/*
 * Generate an ETag from file info.
 * Buffer should be at least 64 bytes.
 */
void utils_generate_etag(const FileInfo* info, char* buffer, size_t buffer_size);

/*
 * URL decode a string in-place.
 */
void utils_url_decode(char* str);

#endif /* UTILS_H */

