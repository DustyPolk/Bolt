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
 * Check if a character is safe for a path component.
 */
static bool is_safe_path_char(char c) {
    return isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == ' ';
}

/*
 * Normalize path by resolving .. and . components safely.
 * Returns false if path attempts to escape root.
 */
static bool normalize_path_components(char* path, size_t path_size) {
    if (!path || path_size == 0) return false;
    
    /* Split into components and resolve */
    char components[64][BOLT_MAX_PATH_LENGTH];
    int comp_count = 0;
    const char* p = path;
    char* comp = components[0];
    size_t comp_len = 0;
    
    while (*p && comp_count < 64) {
        if (*p == '/' || *p == '\\') {
            if (comp_len > 0) {
                comp[comp_len] = '\0';
                comp_count++;
                comp = components[comp_count];
                comp_len = 0;
            }
            p++;
            continue;
        }
        
        if (comp_len < BOLT_MAX_PATH_LENGTH - 1) {
            comp[comp_len++] = *p;
        }
        p++;
    }
    
    if (comp_len > 0) {
        comp[comp_len] = '\0';
        comp_count++;
    }
    
    /* Resolve .. and . */
    int write_idx = 0;
    for (int i = 0; i < comp_count; i++) {
        if (strcmp(components[i], ".") == 0) {
            continue;  /* Skip current directory */
        }
        if (strcmp(components[i], "..") == 0) {
            if (write_idx > 0) {
                write_idx--;  /* Go up one level */
            } else {
                return false;  /* Attempted to escape root */
            }
        } else {
            if (write_idx < 64) {
                strncpy(components[write_idx], components[i], BOLT_MAX_PATH_LENGTH - 1);
                components[write_idx][BOLT_MAX_PATH_LENGTH - 1] = '\0';
                write_idx++;
            }
        }
    }
    
    /* Rebuild path */
    path[0] = '\0';
    for (int i = 0; i < write_idx; i++) {
        if (i > 0) {
            strncat(path, "\\", path_size - strlen(path) - 1);
        }
        strncat(path, components[i], path_size - strlen(path) - 1);
    }
    
    return true;
}

/*
 * Sanitize a URL path to prevent directory traversal attacks.
 * SECURITY: This function must prevent all path traversal attempts.
 */
bool utils_sanitize_path(const char* uri, char* out_path, size_t out_size) {
    if (!uri || !out_path || out_size < 2) {
        return false;
    }
    
    /* Start with web root - use safe copy */
    size_t root_len = strlen(BOLT_WEB_ROOT);
    if (root_len >= out_size) {
        return false;
    }
    
    strncpy(out_path, BOLT_WEB_ROOT, out_size - 1);
    out_path[out_size - 1] = '\0';
    size_t current_len = root_len;
    
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
    
    strncpy(decoded, uri, sizeof(decoded) - 1);
    decoded[sizeof(decoded) - 1] = '\0';
    
    /* URL decode FIRST */
    utils_url_decode(decoded);
    
    /* Check for null bytes AFTER decoding (truncation attack) */
    for (size_t i = 0; decoded[i] != '\0'; i++) {
        if (decoded[i] == '\0') {
            return false;
        }
    }
    
    /* Check for dangerous patterns AFTER decoding */
    if (strstr(decoded, "..") != NULL) {
        return false;  /* Directory traversal attempt */
    }
    if (strstr(decoded, "//") != NULL || strstr(decoded, "\\\\") != NULL) {
        return false;  /* Double slash/backslash */
    }
    
    /* Check for Windows path traversal patterns */
    if (strstr(decoded, "\\..") != NULL || strstr(decoded, "..\\") != NULL) {
        return false;
    }
    
    /* Validate characters - only allow safe characters */
    const char* p = decoded;
    if (*p == '/') p++;  /* Skip leading slash */
    
    while (*p) {
        char c = *p;
        if (!is_safe_path_char(c) && c != '/' && c != '\\') {
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
    
    /* Extract URI path component */
    const char* uri_start = decoded;
    if (*uri_start == '/') {
        uri_start++;  /* Skip leading slash */
    }
    
    size_t uri_start_len = strlen(uri_start);
    if (uri_start_len == 0) {
        return true;  /* Just root */
    }
    
    /* Check total length before building */
    if (current_len + 1 + uri_start_len >= out_size) {
        return false;  /* Path too long */
    }
    
    /* Add separator if needed */
    if (current_len > 0 && out_path[current_len - 1] != '/' && 
        out_path[current_len - 1] != '\\') {
        out_path[current_len++] = '\\';
        out_path[current_len] = '\0';
    }
    
    /* Append URI path component safely */
    strncpy(out_path + current_len, uri_start, out_size - current_len - 1);
    out_path[out_size - 1] = '\0';
    
    /* Convert forward slashes to backslashes for Windows */
    for (char* c = out_path; *c; c++) {
        if (*c == '/') *c = '\\';
    }
    
    /* Normalize path components (resolve . and ..) - only normalize the URI part */
    /* Extract URI part for normalization */
    char uri_part[BOLT_MAX_PATH_LENGTH];
    size_t uri_part_len = strlen(out_path + root_len);
    if (uri_part_len >= sizeof(uri_part)) {
        return false;  /* URI part too long */
    }
    strncpy(uri_part, out_path + root_len, sizeof(uri_part) - 1);
    uri_part[sizeof(uri_part) - 1] = '\0';
    
    /* Normalize the URI part */
    if (!normalize_path_components(uri_part, sizeof(uri_part))) {
        return false;  /* Path normalization failed (attempted escape) */
    }
    
    /* Rebuild full path with normalized URI part */
    size_t normalized_len = strlen(uri_part);
    if (root_len + normalized_len >= out_size) {
        return false;  /* Path too long after normalization */
    }
    
    /* Ensure root is still there */
    strncpy(out_path, BOLT_WEB_ROOT, out_size - 1);
    out_path[root_len] = '\0';
    
    /* Append normalized URI part */
    if (normalized_len > 0) {
        if (root_len > 0 && out_path[root_len - 1] != '\\') {
            strncat(out_path, "\\", out_size - strlen(out_path) - 1);
        }
        strncat(out_path, uri_part, out_size - strlen(out_path) - 1);
    }
    
    /* Final check: ensure path still starts with web root */
    if (strncmp(out_path, BOLT_WEB_ROOT, root_len) != 0) {
        return false;  /* Path escaped root during normalization */
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

