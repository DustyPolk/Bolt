#include "../include/http.h"
#include "../include/bolt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Status code to text mapping.
 */
const char* http_status_text(HttpStatus status) {
    switch (status) {
        case HTTP_200_OK:               return "OK";
        case HTTP_206_PARTIAL_CONTENT:  return "Partial Content";
        case HTTP_304_NOT_MODIFIED:     return "Not Modified";
        case HTTP_400_BAD_REQUEST:      return "Bad Request";
        case HTTP_403_FORBIDDEN:        return "Forbidden";
        case HTTP_404_NOT_FOUND:        return "Not Found";
        case HTTP_405_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_408_REQUEST_TIMEOUT:   return "Request Timeout";
        case HTTP_413_PAYLOAD_TOO_LARGE: return "Payload Too Large";
        case HTTP_414_URI_TOO_LONG:     return "URI Too Long";
        case HTTP_416_RANGE_NOT_SATISFIABLE: return "Range Not Satisfiable";
        case HTTP_500_INTERNAL_ERROR:   return "Internal Server Error";
        default:                        return "Unknown";
    }
}

/*
 * Parse HTTP method from string.
 */
static HttpMethod parse_method(const char* str, size_t len) {
    if (len == 3 && strncmp(str, "GET", 3) == 0) {
        return HTTP_GET;
    }
    if (len == 4 && strncmp(str, "HEAD", 4) == 0) {
        return HTTP_HEAD;
    }
    if (len == 4 && strncmp(str, "POST", 4) == 0) {
        return HTTP_POST;
    }
    if (len == 7 && strncmp(str, "OPTIONS", 7) == 0) {
        return HTTP_OPTIONS;
    }
    return HTTP_UNKNOWN;
}

/*
 * Sanitize a header value by removing CR/LF characters to prevent header injection.
 * Returns sanitized length.
 */
static size_t sanitize_header_value(const char* value, char* out, size_t out_size) {
    if (!value || !out || out_size == 0) return 0;
    
    size_t len = 0;
    for (const char* p = value; *p && len < out_size - 1; p++) {
        char c = *p;
        /* Reject CR, LF, and other control characters */
        if (c == '\r' || c == '\n' || (c < 0x20 && c != '\t')) {
            continue;  /* Skip dangerous characters */
        }
        out[len++] = c;
    }
    out[len] = '\0';
    return len;
}

/*
 * Extract a header value from the request.
 */
static void extract_header(const char* request, const char* header_name,
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
    
    /* Copy and sanitize value */
    size_t value_len = value_end - value_start;
    if (value_len >= out_size) {
        value_len = out_size - 1;
    }
    
    char temp[256];
    size_t temp_len = value_len < sizeof(temp) ? value_len : sizeof(temp) - 1;
    strncpy(temp, value_start, temp_len);
    temp[temp_len] = '\0';
    
    /* Sanitize to prevent header injection */
    sanitize_header_value(temp, out_value, out_size);
}

/*
 * Parse an HTTP request.
 */
HttpRequest http_parse_request(const char* raw_request, size_t length) {
    HttpRequest req = { HTTP_UNKNOWN, "", "", "", "", { 0, SIZE_MAX, false }, false };
    
    if (!raw_request || length == 0) {
        return req;
    }
    
    /* Find the first line end */
    const char* line_end = strstr(raw_request, "\r\n");
    if (!line_end) {
        line_end = strstr(raw_request, "\n");
        if (!line_end) {
            return req;
        }
    }
    
    /* Parse: METHOD URI HTTP/VERSION */
    const char* method_start = raw_request;
    const char* method_end = method_start;
    while (method_end < line_end && *method_end != ' ') {
        method_end++;
    }
    
    if (method_end >= line_end) {
        return req;
    }
    
    req.method = parse_method(method_start, method_end - method_start);
    
    /* Skip space */
    const char* uri_start = method_end + 1;
    
    /* Find URI end */
    const char* uri_end = uri_start;
    while (uri_end < line_end && *uri_end != ' ' && *uri_end != '?') {
        uri_end++;
    }
    
    /* Validate HTTP version (required for security) */
    const char* version_start = uri_end;
    while (version_start < line_end && *version_start == ' ') {
        version_start++;
    }
    
    if (version_start < line_end) {
        /* Check for HTTP/1.0 or HTTP/1.1 */
        if (strncmp(version_start, "HTTP/1.", 7) == 0) {
            char version_char = version_start[7];
            if (version_char != '0' && version_char != '1') {
                return req;  /* Invalid HTTP version - reject HTTP/1.2+ and malformed */
            }
        } else {
            /* No valid HTTP version found - could be HTTP/0.9 or malformed */
            /* For security, require HTTP/1.0 or HTTP/1.1 */
            return req;
        }
    } else {
        /* No version found - reject HTTP/0.9 */
        return req;
    }
    
    /* Copy URI */
    size_t uri_len = uri_end - uri_start;
    if (uri_len >= sizeof(req.uri)) {
        uri_len = sizeof(req.uri) - 1;
    }
    
    strncpy(req.uri, uri_start, uri_len);
    req.uri[uri_len] = '\0';
    
    /* Check URI length */
    if (uri_len > BOLT_MAX_URI_LENGTH) {
        return req;  /* Keep valid = false */
    }
    
    /* Extract caching headers */
    extract_header(raw_request, "If-None-Match", 
                   req.if_none_match, sizeof(req.if_none_match));
    extract_header(raw_request, "If-Modified-Since",
                   req.if_modified_since, sizeof(req.if_modified_since));
    
    /* Extract Accept-Encoding for compression */
    extract_header(raw_request, "Accept-Encoding",
                   req.accept_encoding, sizeof(req.accept_encoding));
    
    /* Extract Range header (will be parsed later when file size is known) */
    char range_header[128];
    extract_header(raw_request, "Range", range_header, sizeof(range_header));
    
    /* Store range header string for later parsing (we need file size first) */
    /* For now, initialize range as invalid - will be parsed in file_server */
    req.range.valid = false;
    req.range.start = 0;
    req.range.end = SIZE_MAX;
    
    req.valid = true;
    return req;
}

/*
 * Send HTTP response headers.
 * SECURITY: All header values are sanitized to prevent header injection.
 */
void http_send_headers(SOCKET client, HttpStatus status, const char* content_type,
                       size_t content_length, const char* extra_headers) {
    char headers[BOLT_MAX_HEADER_SIZE];
    int offset = 0;
    
    /* Status line */
    offset += snprintf(headers + offset, sizeof(headers) - offset,
                       "HTTP/1.1 %d %s\r\n", status, http_status_text(status));
    
    /* Standard headers */
    offset += snprintf(headers + offset, sizeof(headers) - offset,
                       "Server: " BOLT_SERVER_NAME "\r\n"
                       "Connection: keep-alive\r\n"
                       "Keep-Alive: timeout=60, max=1000\r\n"
                       "X-Frame-Options: DENY\r\n"
                       "X-Content-Type-Options: nosniff\r\n"
                       "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self' data:\r\n"
                       "Referrer-Policy: strict-origin-when-cross-origin\r\n"
                       "Permissions-Policy: geolocation=(), microphone=(), camera=()\r\n");
    
    /* Content-Type - sanitize to prevent header injection */
    if (content_type) {
        char sanitized_ct[256];
        sanitize_header_value(content_type, sanitized_ct, sizeof(sanitized_ct));
        offset += snprintf(headers + offset, sizeof(headers) - offset,
                           "Content-Type: %s\r\n", sanitized_ct);
    }
    
    /* Content-Length */
    if (content_length > 0) {
        offset += snprintf(headers + offset, sizeof(headers) - offset,
                           "Content-Length: %zu\r\n", content_length);
    }
    
    /* Extra headers - sanitize to prevent header injection */
    if (extra_headers && *extra_headers) {
        char sanitized_extra[512];
        sanitize_header_value(extra_headers, sanitized_extra, sizeof(sanitized_extra));
        offset += snprintf(headers + offset, sizeof(headers) - offset,
                           "%s", sanitized_extra);
    }
    
    /* End of headers */
    offset += snprintf(headers + offset, sizeof(headers) - offset, "\r\n");
    
    send(client, headers, offset, 0);
}

/*
 * Send an HTTP error response.
 * SECURITY: Generic error message to prevent information disclosure.
 */
void http_send_error(SOCKET client, HttpStatus status) {
    char body[512];
    /* Generic error message - no version info or internal details */
    snprintf(body, sizeof(body),
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head><title>Error %d</title></head>\n"
             "<body>\n"
             "<h1>Error %d</h1>\n"
             "<p>The request could not be processed.</p>\n"
             "</body>\n"
             "</html>\n",
             status, status);
    
    http_send_response(client, status, "text/html; charset=utf-8",
                       body, strlen(body));
}

/*
 * Send a complete response with body.
 */
void http_send_response(SOCKET client, HttpStatus status, const char* content_type,
                        const char* body, size_t body_length) {
    http_send_headers(client, status, content_type, body_length, NULL);
    
    if (body && body_length > 0) {
        send(client, body, (int)body_length, 0);
    }
}

/*
 * Parse Range header from request.
 * Supports formats: "bytes=start-end", "bytes=start-", "bytes=-suffix"
 */
HttpRange http_parse_range(const char* range_header, size_t file_size) {
    HttpRange range = { 0, 0, false };
    
    if (!range_header || file_size == 0) {
        return range;
    }
    
    /* Find "bytes=" prefix */
    const char* bytes_prefix = strstr(range_header, "bytes=");
    if (!bytes_prefix) {
        return range;
    }
    
    const char* range_spec = bytes_prefix + 6; /* Skip "bytes=" */
    
    /* Skip whitespace */
    while (*range_spec == ' ' || *range_spec == '\t') {
        range_spec++;
    }
    
    /* Parse range specification */
    if (*range_spec == '-') {
        /* Suffix length: "bytes=-suffix" */
        range_spec++;
        unsigned long long suffix = strtoull(range_spec, NULL, 10);
        if (suffix > 0 && suffix <= file_size) {
            range.start = file_size - (size_t)suffix;
            range.end = file_size - 1;
            range.valid = true;
        }
    } else {
        /* Start position: "bytes=start-end" or "bytes=start-" */
        unsigned long long start = strtoull(range_spec, NULL, 10);
        if (start >= file_size) {
            return range; /* Invalid - start beyond file */
        }
        
        range.start = (size_t)start;
        
        /* Find end position */
        const char* dash = strchr(range_spec, '-');
        if (dash && dash[1] != '\0' && dash[1] != '\r' && dash[1] != '\n') {
            /* Explicit end: "bytes=start-end" */
            unsigned long long end = strtoull(dash + 1, NULL, 10);
            if (end >= file_size) {
                end = file_size - 1;
            }
            range.end = (size_t)end;
        } else {
            /* No end specified: "bytes=start-" means to end of file */
            range.end = file_size - 1;
        }
        
        if (range.end >= range.start) {
            range.valid = true;
        }
    }
    
    return range;
}

