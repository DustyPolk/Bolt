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
        case HTTP_304_NOT_MODIFIED:     return "Not Modified";
        case HTTP_400_BAD_REQUEST:      return "Bad Request";
        case HTTP_403_FORBIDDEN:        return "Forbidden";
        case HTTP_404_NOT_FOUND:        return "Not Found";
        case HTTP_405_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_413_PAYLOAD_TOO_LARGE: return "Payload Too Large";
        case HTTP_414_URI_TOO_LONG:     return "URI Too Long";
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
    return HTTP_UNKNOWN;
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
    
    /* Copy value */
    size_t value_len = value_end - value_start;
    if (value_len >= out_size) {
        value_len = out_size - 1;
    }
    
    strncpy(out_value, value_start, value_len);
    out_value[value_len] = '\0';
}

/*
 * Parse an HTTP request.
 */
HttpRequest http_parse_request(const char* raw_request, size_t length) {
    HttpRequest req = { HTTP_UNKNOWN, "", "", "", false };
    
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
    
    req.valid = true;
    return req;
}

/*
 * Send HTTP response headers.
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
                       "X-Content-Type-Options: nosniff\r\n");
    
    /* Content-Type */
    if (content_type) {
        offset += snprintf(headers + offset, sizeof(headers) - offset,
                           "Content-Type: %s\r\n", content_type);
    }
    
    /* Content-Length */
    if (content_length > 0) {
        offset += snprintf(headers + offset, sizeof(headers) - offset,
                           "Content-Length: %zu\r\n", content_length);
    }
    
    /* Extra headers */
    if (extra_headers && *extra_headers) {
        offset += snprintf(headers + offset, sizeof(headers) - offset,
                           "%s", extra_headers);
    }
    
    /* End of headers */
    offset += snprintf(headers + offset, sizeof(headers) - offset, "\r\n");
    
    send(client, headers, offset, 0);
}

/*
 * Send an HTTP error response.
 */
void http_send_error(SOCKET client, HttpStatus status) {
    char body[512];
    snprintf(body, sizeof(body),
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head><title>%d %s</title></head>\n"
             "<body>\n"
             "<h1>%d %s</h1>\n"
             "<hr>\n"
             "<p>⚡ Bolt HTTP Server</p>\n"
             "</body>\n"
             "</html>\n",
             status, http_status_text(status),
             status, http_status_text(status));
    
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

