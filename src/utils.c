#include "../include/utils.h"
#include "../include/bolt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/*
 * URL decode a string in-place.
 * Converts %XX hex codes and + to spaces.
 */
void utils_url_decode(char* str) {
    char* src = str;
    char* dst = str;
    
    while (*src) {
        if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            /* Decode hex */
            char hex[3] = { src[1], src[2], '\0' };
            *dst = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst = ' ';
            src++;
        } else {
            *dst = *src;
            src++;
        }
        dst++;
    }
    *dst = '\0';
}

/*
 * Sanitize a URL path to prevent directory traversal attacks.
 */
bool utils_sanitize_path(const char* uri, char* out_path, size_t out_size) {
    if (!uri || !out_path || out_size < 2) {
        return false;
    }
    
    /* Start with web root */
    size_t root_len = strlen(BOLT_WEB_ROOT);
    if (root_len + 2 >= out_size) {
        return false;
    }
    
    strcpy(out_path, BOLT_WEB_ROOT);
    
    /* Handle root URI */
    if (uri[0] == '\0' || (uri[0] == '/' && uri[1] == '\0')) {
        return true;
    }
    
    /* Work with a copy so we can URL decode */
    char decoded[BOLT_MAX_PATH_LENGTH];
    size_t uri_len = strlen(uri);
    if (uri_len >= BOLT_MAX_PATH_LENGTH) {
        return false;
    }
    strcpy(decoded, uri);
    utils_url_decode(decoded);
    
    /* Check for dangerous patterns BEFORE processing */
    if (strstr(decoded, "..") != NULL) {
        return false;  /* Directory traversal attempt */
    }
    if (strstr(decoded, "//") != NULL) {
        return false;  /* Double slash */
    }
    
    /* Check for null bytes (truncation attack) */
    for (size_t i = 0; i < strlen(decoded); i++) {
        if (decoded[i] == '\0') {
            return false;
        }
    }
    
    /* Validate characters - only allow safe characters */
    const char* p = decoded;
    if (*p == '/') p++;  /* Skip leading slash */
    
    while (*p) {
        char c = *p;
        bool valid = isalnum((unsigned char)c) || 
                     c == '/' || c == '.' || c == '-' || c == '_' || c == ' ';
        if (!valid) {
            return false;
        }
        p++;
    }
    
    /* Don't allow paths starting with a dot (hidden files) */
    p = decoded;
    if (*p == '/') p++;
    if (*p == '.') {
        return false;
    }
    
    /* Build the full path */
    const char* uri_start = decoded;
    if (*uri_start == '/') {
        uri_start++;  /* Skip leading slash for concatenation */
    }
    
    if (strlen(out_path) + 1 + strlen(uri_start) >= out_size) {
        return false;  /* Path too long */
    }
    
    /* Add separator if needed */
    size_t current_len = strlen(out_path);
    if (current_len > 0 && out_path[current_len - 1] != '/' && 
        out_path[current_len - 1] != '\\') {
        strcat(out_path, "/");
    }
    
    strcat(out_path, uri_start);
    
    /* Convert forward slashes to backslashes for Windows */
    for (char* c = out_path; *c; c++) {
        if (*c == '/') *c = '\\';
    }
    
    return true;
}

/*
 * Get file information.
 */
FileInfo utils_get_file_info(const char* path) {
    FileInfo info = { false, false, 0, 0 };
    
    if (!path) {
        return info;
    }
    
    struct _stat st;
    if (_stat(path, &st) == 0) {
        info.exists = true;
        info.is_directory = (st.st_mode & _S_IFDIR) != 0;
        info.size = (size_t)st.st_size;
        info.mtime = st.st_mtime;
    }
    
    return info;
}

/*
 * Get the file extension from a path.
 */
const char* utils_get_extension(const char* path) {
    if (!path) {
        return "";
    }
    
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) {
        return "";
    }
    
    /* Make sure the dot is after any path separators */
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    const char* last_sep = slash > backslash ? slash : backslash;
    
    if (last_sep && dot < last_sep) {
        return "";  /* Dot is in directory name, not file extension */
    }
    
    return dot + 1;  /* Return extension without the dot */
}

/*
 * Format a file size as human-readable string.
 */
void utils_format_size(size_t size, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 16) {
        return;
    }
    
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int unit_index = 0;
    double display_size = (double)size;
    
    while (display_size >= 1024.0 && unit_index < 4) {
        display_size /= 1024.0;
        unit_index++;
    }
    
    if (unit_index == 0) {
        snprintf(buffer, buffer_size, "%zu %s", size, units[unit_index]);
    } else {
        snprintf(buffer, buffer_size, "%.1f %s", display_size, units[unit_index]);
    }
}

/*
 * Format a timestamp as HTTP date (RFC 7231).
 * Example: "Sun, 06 Nov 1994 08:49:37 GMT"
 */
void utils_format_http_date(time_t timestamp, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 30) {
        return;
    }
    
    struct tm* gmt = gmtime(&timestamp);
    if (gmt) {
        strftime(buffer, buffer_size, "%a, %d %b %Y %H:%M:%S GMT", gmt);
    } else {
        buffer[0] = '\0';
    }
}

/*
 * Generate an ETag from file info.
 * Uses file size and modification time for uniqueness.
 */
void utils_generate_etag(const FileInfo* info, char* buffer, size_t buffer_size) {
    if (!info || !buffer || buffer_size < 32) {
        return;
    }
    
    /* Create ETag from size and mtime */
    snprintf(buffer, buffer_size, "\"%zx-%lx\"", 
             info->size, (unsigned long)info->mtime);
}

