#ifndef HTTP_H
#define HTTP_H

#include <winsock2.h>
#include <stdbool.h>

/*
 * HTTP request parsing and response building.
 */

/* HTTP Methods */
typedef enum {
    HTTP_GET,
    HTTP_HEAD,
    HTTP_POST,
    HTTP_OPTIONS,
    HTTP_UNKNOWN
} HttpMethod;

/* HTTP Status Codes */
typedef enum {
    HTTP_200_OK = 200,
    HTTP_206_PARTIAL_CONTENT = 206,
    HTTP_304_NOT_MODIFIED = 304,
    HTTP_400_BAD_REQUEST = 400,
    HTTP_403_FORBIDDEN = 403,
    HTTP_404_NOT_FOUND = 404,
    HTTP_405_METHOD_NOT_ALLOWED = 405,
    HTTP_408_REQUEST_TIMEOUT = 408,
    HTTP_413_PAYLOAD_TOO_LARGE = 413,
    HTTP_414_URI_TOO_LONG = 414,
    HTTP_416_RANGE_NOT_SATISFIABLE = 416,
    HTTP_500_INTERNAL_ERROR = 500
} HttpStatus;

/* Range request specification */
typedef struct {
    size_t start;      /* Start byte (inclusive) */
    size_t end;        /* End byte (inclusive), SIZE_MAX means "to end" */
    bool valid;        /* True if range is valid */
} HttpRange;

/* Parsed HTTP Request */
typedef struct {
    HttpMethod method;
    char uri[2048];
    char if_none_match[64];    /* For ETag caching */
    char if_modified_since[64]; /* For Last-Modified caching */
    char accept_encoding[128];  /* For compression support */
    HttpRange range;            /* For Range requests */
    bool valid;
} HttpRequest;

/*
 * Parse an HTTP request from raw data.
 * Returns parsed request structure.
 */
HttpRequest http_parse_request(const char* raw_request, size_t length);

/*
 * Send an HTTP error response.
 */
void http_send_error(SOCKET client, HttpStatus status);

/*
 * Send HTTP response headers.
 * content_length can be 0 if unknown.
 */
void http_send_headers(SOCKET client, HttpStatus status, const char* content_type,
                       size_t content_length, const char* extra_headers);

/*
 * Get status text for an HTTP status code.
 */
const char* http_status_text(HttpStatus status);

/*
 * Build and send a complete response with body.
 */
void http_send_response(SOCKET client, HttpStatus status, const char* content_type,
                        const char* body, size_t body_length);

/*
 * Parse Range header from request.
 * Returns parsed range with valid flag set if range is valid.
 */
HttpRange http_parse_range(const char* range_header, size_t file_size);

#endif /* HTTP_H */

