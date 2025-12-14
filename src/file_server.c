#include "../include/file_server.h"
#include "../include/bolt.h"
#include "../include/bolt_server.h"
#include "../include/utils.h"
#include "../include/mime.h"
#include "../include/http.h"
#include "../include/file_sender.h"
#include "../include/file_cache.h"
#include "../include/compression.h"
#include "../include/metrics.h"
#include "../include/vhost.h"
#include "../include/rewrite.h"
#include "../include/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <time.h>
#include <ws2tcpip.h>

/*
 * Check if a cached response is still valid.
 */
static bool check_cache_valid(const HttpRequest* request, const FileInfo* info) {
    if (!request || !info) {
        return false;
    }
    
    /* Check ETag */
    if (request->if_none_match[0]) {
        char current_etag[64];
        utils_generate_etag(info, current_etag, sizeof(current_etag));
        if (strcmp(request->if_none_match, current_etag) == 0) {
            return true;
        }
    }
    
    /* Check If-Modified-Since (basic comparison) */
    if (request->if_modified_since[0]) {
        /* For simplicity, just compare the date strings */
        char current_date[64];
        utils_format_http_date(info->mtime, current_date, sizeof(current_date));
        if (strcmp(request->if_modified_since, current_date) == 0) {
            return true;
        }
    }
    
    return false;
}

/*
 * Send cache headers for a file.
 */
static void build_cache_headers(const FileInfo* info, char* buffer, size_t buffer_size) {
    if (!info || !buffer || buffer_size < 128) {
        buffer[0] = '\0';
        return;
    }
    
    char etag[64];
    char last_modified[64];
    
    utils_generate_etag(info, etag, sizeof(etag));
    utils_format_http_date(info->mtime, last_modified, sizeof(last_modified));
    
    snprintf(buffer, buffer_size,
             "ETag: %s\r\n"
             "Last-Modified: %s\r\n"
             "Cache-Control: public, max-age=3600\r\n",
             etag, last_modified);
}

/*
 * Serve a file to the client.
 */
bool file_server_serve_file(SOCKET client, const char* filepath, 
                            const HttpRequest* request) {
    /* Get file info */
    FileInfo info = utils_get_file_info(filepath);
    
    if (!info.exists) {
        http_send_error(client, HTTP_404_NOT_FOUND);
        return false;
    }
    
    if (info.is_directory) {
        /* This shouldn't happen if called correctly */
        http_send_error(client, HTTP_404_NOT_FOUND);
        return false;
    }
    
    /* Check file size limit */
    if (info.size > BOLT_MAX_FILE_SIZE) {
        http_send_error(client, HTTP_413_PAYLOAD_TOO_LARGE);
        return false;
    }
    
    /* Check if client cache is valid */
    if (check_cache_valid(request, &info)) {
        http_send_headers(client, HTTP_304_NOT_MODIFIED, NULL, 0, NULL);
        return true;
    }
    
    /* Get MIME type */
    const char* ext = utils_get_extension(filepath);
    const char* mime_type = mime_get_type(ext);
    
    /* Build content type with charset for text files */
    char content_type[128];
    if (mime_is_text(mime_type)) {
        snprintf(content_type, sizeof(content_type), "%s; charset=utf-8", mime_type);
    } else {
        strncpy(content_type, mime_type, sizeof(content_type) - 1);
        content_type[sizeof(content_type) - 1] = '\0';
    }
    
    /* Build cache headers */
    char cache_headers[256];
    build_cache_headers(&info, cache_headers, sizeof(cache_headers));
    
    /* Open the file */
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        http_send_error(client, HTTP_500_INTERNAL_ERROR);
        return false;
    }
    
    /* Send headers */
    http_send_headers(client, HTTP_200_OK, content_type, info.size, cache_headers);
    
    /* For HEAD requests, don't send body */
    if (request->method == HTTP_HEAD) {
        fclose(file);
        return true;
    }
    
    /* Send file in chunks */
    char chunk[65536];
    size_t bytes_read;
    
    while ((bytes_read = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        int sent = send(client, chunk, (int)bytes_read, 0);
        if (sent <= 0) {
            fclose(file);
            return false;
        }
    }
    
    fclose(file);
    return true;
}

/*
 * Generate and serve a directory listing.
 */
void file_server_serve_directory(SOCKET client, const char* dirpath,
                                 const char* uri) {
#if !BOLT_ENABLE_DIR_LISTING
    BOLT_UNUSED(dirpath);
    BOLT_UNUSED(uri);
    http_send_error(client, HTTP_404_NOT_FOUND);
    return;
#else
    WIN32_FIND_DATAA find_data;
    char search_path[BOLT_MAX_PATH_LENGTH];
    
    /* Build search pattern */
    snprintf(search_path, sizeof(search_path), "%s\\*", dirpath);
    
    HANDLE find_handle = FindFirstFileA(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        http_send_error(client, HTTP_404_NOT_FOUND);
        return;
    }
    
    /* Build HTML response */
    char* html = (char*)malloc(64 * 1024);  /* 64KB buffer for listing */
    if (!html) {
        FindClose(find_handle);
        http_send_error(client, HTTP_500_INTERNAL_ERROR);
        return;
    }
    
    int offset = 0;
    
    /* HTML header */
    offset += snprintf(html + offset, 64 * 1024 - offset,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <meta charset=\"utf-8\">\n"
        "  <title>Index of %s</title>\n"
        "  <style>\n"
        "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 40px; background: #f5f5f5; }\n"
        "    h1 { color: #333; border-bottom: 2px solid #007acc; padding-bottom: 10px; }\n"
        "    table { border-collapse: collapse; width: 100%%; max-width: 900px; background: white; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }\n"
        "    th, td { padding: 12px 16px; text-align: left; border-bottom: 1px solid #eee; }\n"
        "    th { background: #007acc; color: white; }\n"
        "    tr:hover { background: #f8f8f8; }\n"
        "    a { color: #007acc; text-decoration: none; }\n"
        "    a:hover { text-decoration: underline; }\n"
        "    .size { text-align: right; color: #666; }\n"
        "    .date { color: #888; }\n"
        "    .dir { font-weight: bold; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>Index of %s</h1>\n"
        "  <table>\n"
        "    <tr><th>Name</th><th>Size</th><th>Modified</th></tr>\n",
        uri, uri);
    
    /* Parent directory link (if not root) */
    if (strcmp(uri, "/") != 0 && strlen(uri) > 1) {
        offset += snprintf(html + offset, 64 * 1024 - offset,
            "    <tr><td class=\"dir\"><a href=\"../\">..</a></td>"
            "<td class=\"size\">-</td><td class=\"date\">-</td></tr>\n");
    }
    
    /* List entries */
    do {
        /* Skip . and .. */
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        /* Skip hidden files */
        if (find_data.cFileName[0] == '.') {
            continue;
        }
        
        bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        
        /* Format size */
        char size_str[32];
        if (is_dir) {
            strcpy(size_str, "-");
        } else {
            ULARGE_INTEGER file_size;
            file_size.LowPart = find_data.nFileSizeLow;
            file_size.HighPart = find_data.nFileSizeHigh;
            utils_format_size((size_t)file_size.QuadPart, size_str, sizeof(size_str));
        }
        
        /* Format date */
        char date_str[64];
        FILETIME local_time;
        SYSTEMTIME sys_time;
        FileTimeToLocalFileTime(&find_data.ftLastWriteTime, &local_time);
        FileTimeToSystemTime(&local_time, &sys_time);
        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d %02d:%02d",
                 sys_time.wYear, sys_time.wMonth, sys_time.wDay,
                 sys_time.wHour, sys_time.wMinute);
        
        /* Build link */
        offset += snprintf(html + offset, 64 * 1024 - offset,
            "    <tr><td class=\"%s\"><a href=\"%s%s%s\">%s%s</a></td>"
            "<td class=\"size\">%s</td><td class=\"date\">%s</td></tr>\n",
            is_dir ? "dir" : "",
            uri[strlen(uri) - 1] == '/' ? "" : "/",
            find_data.cFileName,
            is_dir ? "/" : "",
            find_data.cFileName,
            is_dir ? "/" : "",
            size_str, date_str);
        
        /* Prevent buffer overflow */
        if (offset > 60 * 1024) {
            break;
        }
        
    } while (FindNextFileA(find_handle, &find_data));
    
    FindClose(find_handle);
    
    /* HTML footer */
    offset += snprintf(html + offset, 64 * 1024 - offset,
        "  </table>\n"
        "  <p style=\"color:#888;margin-top:20px;\">⚡ Bolt HTTP Server</p>\n"
        "</body>\n"
        "</html>\n");
    
    /* Send response */
    http_send_response(client, HTTP_200_OK, "text/html; charset=utf-8",
                       html, offset);
    
    free(html);
#endif
}

/*
 * Handle a file serving request.
 */
void file_server_handle(SOCKET client, const HttpRequest* request) {
    if (!request || !request->valid) {
        http_send_error(client, HTTP_400_BAD_REQUEST);
        return;
    }
    
    /* Handle OPTIONS (CORS preflight) */
    if (request->method == HTTP_OPTIONS) {
        http_send_headers(client, HTTP_200_OK, NULL, 0,
                         "Allow: GET, HEAD, OPTIONS\r\n"
                         "Access-Control-Allow-Methods: GET, HEAD, OPTIONS\r\n"
                         "Access-Control-Allow-Headers: Content-Type\r\n");
        return;
    }
    
    /* Only allow GET and HEAD for file serving */
    if (request->method != HTTP_GET && request->method != HTTP_HEAD) {
        http_send_headers(client, HTTP_405_METHOD_NOT_ALLOWED, NULL, 0,
                         "Allow: GET, HEAD, OPTIONS\r\n");
        return;
    }
    
    /* Sanitize the path */
    char filepath[BOLT_MAX_PATH_LENGTH];
    if (!utils_sanitize_path(request->uri, filepath, sizeof(filepath))) {
        http_send_error(client, HTTP_403_FORBIDDEN);
        return;
    }
    
    /* Get file info */
    FileInfo info = utils_get_file_info(filepath);
    
    if (!info.exists) {
        http_send_error(client, HTTP_404_NOT_FOUND);
        return;
    }
    
    if (info.is_directory) {
        /* Try to serve index.html first */
        char index_path[BOLT_MAX_PATH_LENGTH];
        snprintf(index_path, sizeof(index_path), "%s\\" BOLT_INDEX_FILE, filepath);
        
        FileInfo index_info = utils_get_file_info(index_path);
        if (index_info.exists && !index_info.is_directory) {
            file_server_serve_file(client, index_path, request);
        } else {
            /* Serve directory listing */
            file_server_serve_directory(client, filepath, request->uri);
        }
    } else {
        /* Serve the file */
        file_server_serve_file(client, filepath, request);
    }
}

/* =========================
 * Bolt async fast-path
 * ========================= */

/*
 * Extract a header value from the request buffer.
 */
static void extract_header_from_buffer(const char* request, const char* header_name,
                                       char* out_value, size_t out_size) {
    out_value[0] = '\0';
    
    /* Find the header */
    const char* header_start = strstr(request, header_name);
    if (!header_start) {
        return;
    }
    
    /* Skip header name and colon */
    const char* value_start = header_start + strlen(header_name);
    if (*value_start == ':') {
        value_start++;
    }
    
    /* Skip whitespace */
    while (*value_start == ' ' || *value_start == '\t') {
        value_start++;
    }
    
    /* Find end of line */
    const char* value_end = value_start;
    while (*value_end && *value_end != '\r' && *value_end != '\n') {
        value_end++;
    }
    
    /* Copy value */
    size_t value_len = value_end - value_start;
    if (value_len >= out_size) {
        value_len = out_size - 1;
    }
    
    strncpy(out_value, value_start, value_len);
    out_value[value_len] = '\0';
}

/*
 * Sanitize header value to prevent header injection.
 */
static size_t sanitize_header_value(const char* value, char* out, size_t out_size) {
    if (!value || !out || out_size == 0) return 0;
    
    size_t len = 0;
    for (const char* p = value; *p && len < out_size - 1; p++) {
        char c = *p;
        /* Reject CR, LF, and other control characters */
        if (c == '\r' || c == '\n' || (c < 0x20 && c != '\t')) {
            continue;
        }
        out[len++] = c;
    }
    out[len] = '\0';
    return len;
}

static size_t build_headers_206(char* out, size_t out_sz,
                                const char* content_type,
                                size_t range_start,
                                size_t range_end,
                                size_t file_size,
                                const char* extra_headers,
                                bool keep_alive) {
    /* Sanitize header values */
    char safe_ct[256] = "application/octet-stream";
    char safe_extra[512] = "";
    
    if (content_type) {
        sanitize_header_value(content_type, safe_ct, sizeof(safe_ct));
    }
    if (extra_headers && *extra_headers) {
        sanitize_header_value(extra_headers, safe_extra, sizeof(safe_extra));
    }
    
    size_t range_length = range_end - range_start + 1;
    
    int offset = snprintf(out, out_sz,
        "HTTP/1.1 206 Partial Content\r\n"
        "Server: " BOLT_SERVER_NAME "\r\n"
        "Connection: %s\r\n"
        "Keep-Alive: timeout=60, max=1000\r\n"
        "Content-Type: %s\r\n"
        "Content-Range: bytes %zu-%zu/%zu\r\n"
        "Content-Length: %zu\r\n",
        keep_alive ? "keep-alive" : "close",
        safe_ct,
        range_start, range_end, file_size,
        range_length);
    
    offset += snprintf(out + offset, out_sz - offset,
        "%s"
        "X-Frame-Options: DENY\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self' data:\r\n"
        "Referrer-Policy: strict-origin-when-cross-origin\r\n"
        "Permissions-Policy: geolocation=(), microphone=(), camera=()\r\n"
        "\r\n",
        safe_extra);
    
    return (size_t)offset;
}

static size_t build_headers_200(char* out, size_t out_sz,
                                const char* content_type,
                                size_t content_length,
                                const char* extra_headers,
                                bool keep_alive,
                                const char* content_encoding) {
    /* Sanitize header values to prevent injection */
    char safe_ct[256] = "application/octet-stream";
    char safe_extra[512] = "";
    char safe_encoding[64] = "";
    
    if (content_type) {
        sanitize_header_value(content_type, safe_ct, sizeof(safe_ct));
    }
    if (extra_headers && *extra_headers) {
        sanitize_header_value(extra_headers, safe_extra, sizeof(safe_extra));
    }
    if (content_encoding && *content_encoding) {
        sanitize_header_value(content_encoding, safe_encoding, sizeof(safe_encoding));
    }
    
    int offset = snprintf(out, out_sz,
        "HTTP/1.1 200 OK\r\n"
        "Server: " BOLT_SERVER_NAME "\r\n"
        "Connection: %s\r\n"
        "Keep-Alive: timeout=60, max=1000\r\n"
        "Content-Type: %s\r\n",
        keep_alive ? "keep-alive" : "close",
        safe_ct);
    
    if (safe_encoding[0]) {
        offset += snprintf(out + offset, out_sz - offset,
            "Content-Encoding: %s\r\n", safe_encoding);
    }
    
    offset += snprintf(out + offset, out_sz - offset,
        "Content-Length: %zu\r\n"
        "%s"
        "X-Frame-Options: DENY\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self' data:\r\n"
        "Referrer-Policy: strict-origin-when-cross-origin\r\n"
        "Permissions-Policy: geolocation=(), microphone=(), camera=()\r\n"
        "\r\n",
        content_length,
        safe_extra);
    
    return (size_t)offset;
}

static size_t build_headers_status(char* out, size_t out_sz,
                                   HttpStatus status,
                                   const char* content_type,
                                   size_t content_length,
                                   bool keep_alive) {
    /* Sanitize header values to prevent injection */
    char safe_ct[256] = "text/plain; charset=utf-8";
    
    if (content_type) {
        sanitize_header_value(content_type, safe_ct, sizeof(safe_ct));
    }
    
    return (size_t)snprintf(out, out_sz,
        "HTTP/1.1 %d %s\r\n"
        "Server: " BOLT_SERVER_NAME "\r\n"
        "Connection: %s\r\n"
        "Keep-Alive: timeout=60, max=1000\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "X-Frame-Options: DENY\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self' data:\r\n"
        "Referrer-Policy: strict-origin-when-cross-origin\r\n"
        "Permissions-Policy: geolocation=(), microphone=(), camera=()\r\n"
        "\r\n",
        status, http_status_text(status),
        keep_alive ? "keep-alive" : "close",
        safe_ct,
        content_length
    );
}

void send_error_async(BoltConnection* conn, HttpStatus status) {
    char body[256];
    int body_len = snprintf(body, sizeof(body),
                            "%d %s\n", status, http_status_text(status));
    if (body_len < 0) body_len = 0;

    char headers[512];
    size_t hdr_len = build_headers_status(headers, sizeof(headers), status,
                                          "text/plain; charset=utf-8",
                                          (size_t)body_len,
                                          conn->keep_alive);

    /* If this fails, close connection (worker will release on completion path) */
    if (!bolt_send_response(conn, headers, hdr_len, body, (size_t)body_len)) {
        bolt_conn_close(conn);
        bolt_conn_release(g_bolt_server->conn_pool, conn);
    }
}

void bolt_file_server_handle(BoltConnection* conn, const HttpRequest* request) {
    if (!conn || !request || !request->valid) {
        if (conn) send_error_async(conn, HTTP_400_BAD_REQUEST);
        return;
    }
    
    /* Start profiling */
    profiler_start_request(conn);

    /* Handle metrics endpoint */
    if (metrics_is_endpoint(request->uri)) {
        char metrics_json[2048];
        size_t json_len = 0;
        if (metrics_generate_json(g_bolt_server, metrics_json, sizeof(metrics_json), &json_len)) {
            char headers[512];
            size_t hdr_len = snprintf(headers, sizeof(headers),
                "HTTP/1.1 200 OK\r\n"
                "Server: " BOLT_SERVER_NAME "\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %zu\r\n"
                "Cache-Control: no-cache\r\n"
                "\r\n",
                json_len);
            if (bolt_send_response(conn, headers, hdr_len, metrics_json, json_len)) {
                return;
            }
        }
        send_error_async(conn, HTTP_500_INTERNAL_ERROR);
        return;
    }

    /* Handle OPTIONS (CORS preflight) */
    if (request->method == HTTP_OPTIONS) {
        char headers[512];
        size_t hdr_len = snprintf(headers, sizeof(headers),
            "HTTP/1.1 200 OK\r\n"
            "Server: " BOLT_SERVER_NAME "\r\n"
            "Allow: GET, HEAD, OPTIONS\r\n"
            "Access-Control-Allow-Methods: GET, HEAD, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Content-Length: 0\r\n"
            "\r\n");
        if (!bolt_send_headers_only(conn, headers, hdr_len)) {
            bolt_conn_close(conn);
            bolt_conn_release(g_bolt_server->conn_pool, conn);
        }
        return;
    }
    
    /* Handle POST - for static file server, POST is not supported */
    if (request->method == HTTP_POST) {
        send_error_async(conn, HTTP_405_METHOD_NOT_ALLOWED);
        return;
    }
    
    if (request->method != HTTP_GET && request->method != HTTP_HEAD) {
        /* Send 405 with Allow header */
        char headers[512];
        size_t hdr_len = snprintf(headers, sizeof(headers),
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Server: " BOLT_SERVER_NAME "\r\n"
            "Allow: GET, HEAD, OPTIONS\r\n"
            "Content-Length: 0\r\n"
            "\r\n");
        if (!bolt_send_headers_only(conn, headers, hdr_len)) {
            bolt_conn_close(conn);
            bolt_conn_release(g_bolt_server->conn_pool, conn);
        }
        return;
    }

    /* Apply rewrite rules */
    char rewritten_uri[2048];
    bool was_rewritten = false;
    if (g_bolt_server && g_bolt_server->rewrite_engine) {
        was_rewritten = rewrite_apply(g_bolt_server->rewrite_engine, request->uri,
                                      rewritten_uri, sizeof(rewritten_uri));
    }
    
    const char* uri_to_use = was_rewritten ? rewritten_uri : request->uri;
    
    /* Check for redirect */
    if (was_rewritten && g_bolt_server && g_bolt_server->rewrite_engine) {
        BoltRewriteRule* rule = g_bolt_server->rewrite_engine->rules;
        while (rule) {
            if (rewrite_match_pattern(rule->pattern, request->uri)) {
                if (rule->type == REWRITE_REDIRECT_301 || rule->type == REWRITE_REDIRECT_302) {
                    int status = (rule->type == REWRITE_REDIRECT_301) ? 301 : 302;
                    char headers[512];
                    size_t hdr_len = snprintf(headers, sizeof(headers),
                        "HTTP/1.1 %d %s\r\n"
                        "Server: " BOLT_SERVER_NAME "\r\n"
                        "Location: %s\r\n"
                        "Content-Length: 0\r\n"
                        "\r\n",
                        status, status == 301 ? "Moved Permanently" : "Found",
                        rewritten_uri);
                    if (bolt_send_headers_only(conn, headers, hdr_len)) {
                        return;
                    }
                }
                break;
            }
            rule = rule->next;
        }
    }
    
    /* Determine virtual host */
    BoltVHost* vhost = NULL;
    if (g_bolt_server && g_bolt_server->vhost_manager) {
        char host_header[256];
        extract_header_from_buffer(conn->recv_buffer, "Host", host_header, sizeof(host_header));
        vhost = vhost_find(g_bolt_server->vhost_manager, host_header);
        if (!vhost) {
            vhost = vhost_get_default(g_bolt_server->vhost_manager);
        }
    }
    
    /* Determine web root */
    const char* web_root = g_bolt_server ? g_bolt_server->web_root : BOLT_WEB_ROOT;
    if (vhost && vhost->root[0]) {
        web_root = vhost->root;
    }
    
    char filepath[BOLT_MAX_PATH_LENGTH];
    if (!utils_sanitize_path_with_root(uri_to_use, web_root, filepath, sizeof(filepath))) {
        send_error_async(conn, HTTP_403_FORBIDDEN);
        return;
    }

    FileInfo info = utils_get_file_info(filepath);
    if (!info.exists) {
        send_error_async(conn, HTTP_404_NOT_FOUND);
        return;
    }

    if (info.is_directory) {
        /* Try index file */
        char index_path[BOLT_MAX_PATH_LENGTH];
        snprintf(index_path, sizeof(index_path), "%s\\" BOLT_INDEX_FILE, filepath);
        FileInfo index_info = utils_get_file_info(index_path);
        if (index_info.exists && !index_info.is_directory) {
            strncpy(filepath, index_path, sizeof(filepath) - 1);
            filepath[sizeof(filepath) - 1] = '\0';
            info = index_info;
        } else {
#if BOLT_ENABLE_DIR_LISTING
            /* Not performance critical (disabled by default) */
            file_server_serve_directory(conn->socket, filepath, request->uri);
            return;
#else
            send_error_async(conn, HTTP_404_NOT_FOUND);
            return;
#endif
        }
    }

    if (info.size > BOLT_MAX_FILE_SIZE) {
        send_error_async(conn, HTTP_413_PAYLOAD_TOO_LARGE);
        return;
    }

    const char* ext = utils_get_extension(filepath);
    const char* mime_type = mime_get_type(ext);
    char content_type[128];
    if (mime_is_text(mime_type)) {
        snprintf(content_type, sizeof(content_type), "%s; charset=utf-8", mime_type);
    } else {
        strncpy(content_type, mime_type, sizeof(content_type) - 1);
        content_type[sizeof(content_type) - 1] = '\0';
    }

    /* Small-file cache for mixed-site performance */
#if BOLT_ENABLE_FILE_CACHE
    if (g_bolt_server && g_bolt_server->file_cache &&
        request->method != HTTP_HEAD) {
        BoltCachedResponse cached;
        if (bolt_file_cache_get(g_bolt_server->file_cache,
                                filepath,
                                content_type,
                                info.mtime,
                                info.size,
                                &cached)) {
            if (!bolt_send_response(conn, cached.headers, cached.headers_len,
                                    cached.body, cached.body_len)) {
                bolt_conn_close(conn);
                bolt_conn_release(g_bolt_server->conn_pool, conn);
            }
            return;
        }
    }
#endif

    /* Check if compression should be applied */
    BoltCompressionConfig comp_config = compression_get_default_config();
    BoltCompressionType comp_type = BOLT_COMPRESS_NONE;
    const char* content_encoding = NULL;
    bool should_compress = false;
    
    if (compression_should_compress(content_type, &comp_config) &&
        info.size >= comp_config.min_size) {
        comp_type = compression_parse_accept_encoding(request->accept_encoding, &comp_config);
        if (comp_type == BOLT_COMPRESS_GZIP) {
            content_encoding = "gzip";
            should_compress = true;
        }
    }
    
    /* For small files that fit in send buffer, compress in memory */
    if (should_compress && info.size <= BOLT_SEND_BUFFER_SIZE / 2) {
        /* Read file into memory */
        HANDLE file = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            char* file_data = (char*)malloc(info.size);
            if (file_data) {
                DWORD bytes_read = 0;
                if (ReadFile(file, file_data, (DWORD)info.size, &bytes_read, NULL) &&
                    bytes_read == info.size) {
                    /* Compress */
                    BoltCompressedData compressed;
                    if (compression_gzip(file_data, info.size, &compressed, comp_config.level)) {
                        /* Build headers with compression */
                        char cache_headers[256];
                        build_cache_headers(&info, cache_headers, sizeof(cache_headers));
                        
                        char headers[1024];
                        size_t hdr_len = build_headers_200(headers, sizeof(headers),
                                                           content_type,
                                                           compressed.size,
                                                           cache_headers,
                                                           conn->keep_alive,
                                                           content_encoding);
                        
                        if (request->method == HTTP_HEAD) {
                            free(compressed.data);
                            free(file_data);
                            CloseHandle(file);
                            if (!bolt_send_headers_only(conn, headers, hdr_len)) {
                                bolt_conn_close(conn);
                                bolt_conn_release(g_bolt_server->conn_pool, conn);
                            }
                            return;
                        }
                        
                        /* Send compressed data */
                        if (bolt_send_response(conn, headers, hdr_len,
                                               compressed.data, compressed.size)) {
                            free(compressed.data);
                            free(file_data);
                            CloseHandle(file);
                            return;
                        }
                        free(compressed.data);
                    }
                }
                free(file_data);
            }
            CloseHandle(file);
        }
        /* Fall through to uncompressed send if compression failed */
    }
    
    /* Parse Range header if present */
    char range_header[128];
    extract_header_from_buffer(conn->recv_buffer, "Range", range_header, sizeof(range_header));
    HttpRange range = { 0, SIZE_MAX, false };
    
    if (range_header[0] != '\0') {
        range = http_parse_range(range_header, info.size);
        if (!range.valid) {
            /* Invalid range - send 416 Range Not Satisfiable */
            char headers[512];
            size_t hdr_len = snprintf(headers, sizeof(headers),
                "HTTP/1.1 416 Range Not Satisfiable\r\n"
                "Server: " BOLT_SERVER_NAME "\r\n"
                "Content-Range: bytes */%zu\r\n"
                "Content-Length: 0\r\n"
                "\r\n",
                info.size);
            if (!bolt_send_headers_only(conn, headers, hdr_len)) {
                bolt_conn_close(conn);
                bolt_conn_release(g_bolt_server->conn_pool, conn);
            }
            return;
        }
    }
    
    /* Build cache headers (generated per request for non-cached path) */
    char cache_headers[256];
    build_cache_headers(&info, cache_headers, sizeof(cache_headers));

    char headers[1024];
    size_t hdr_len;
    
    if (range.valid) {
        /* Range request - build 206 Partial Content headers */
        hdr_len = build_headers_206(headers, sizeof(headers),
                                    content_type,
                                    range.start,
                                    range.end,
                                    info.size,
                                    cache_headers,
                                    conn->keep_alive);
    } else {
        /* Full file request */
        hdr_len = build_headers_200(headers, sizeof(headers),
                                    content_type,
                                    info.size,
                                    cache_headers,
                                    conn->keep_alive,
                                    NULL);
    }

    if (request->method == HTTP_HEAD) {
        if (!bolt_send_headers_only(conn, headers, hdr_len)) {
            bolt_conn_close(conn);
            bolt_conn_release(g_bolt_server->conn_pool, conn);
        }
        return;
    }

    /* Zero-copy file send (with range support) */
    if (!bolt_send_file(conn, filepath, headers, hdr_len, range.valid ? &range : NULL)) {
        send_error_async(conn, HTTP_500_INTERNAL_ERROR);
        return;
    }
}

