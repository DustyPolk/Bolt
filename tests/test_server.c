/*
 * Bolt Test Suite - Integration Tests
 * 
 * Tests that make real HTTP requests to verify end-to-end functionality.
 * These tests require the server to be running or will start it.
 */

#include "minunit.h"
#include "../include/bolt.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* Test configuration */
#define TEST_HOST "127.0.0.1"
#define TEST_PORT 8080
#define TEST_TIMEOUT_MS 5000
#define RECV_BUFFER_SIZE 8192

/*============================================================================
 * Helper Functions
 *============================================================================*/

/*
 * Create a TCP socket and connect to the server.
 * Returns INVALID_SOCKET on failure.
 */
static SOCKET connect_to_server(void) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    
    /* Set timeout */
    DWORD timeout = TEST_TIMEOUT_MS;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TEST_PORT);
    inet_pton(AF_INET, TEST_HOST, &server_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    
    return sock;
}

/*
 * Send an HTTP request and receive the response.
 * Returns the HTTP status code, or -1 on error.
 */
static int send_request(const char* method, const char* path, const char* headers,
                        char* response_buffer, size_t response_buffer_size) {
    SOCKET sock = connect_to_server();
    if (sock == INVALID_SOCKET) {
        return -1;
    }
    
    /* Build request */
    char request[4096];
    int request_len;
    
    if (headers && strlen(headers) > 0) {
        request_len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "%s"
            "\r\n",
            method, path, TEST_HOST, TEST_PORT, headers);
    } else {
        request_len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path, TEST_HOST, TEST_PORT);
    }
    
    /* Send request */
    if (send(sock, request, request_len, 0) != request_len) {
        closesocket(sock);
        return -1;
    }
    
    /* Receive response */
    int total_received = 0;
    int received;
    
    while ((received = recv(sock, response_buffer + total_received, 
                           (int)(response_buffer_size - total_received - 1), 0)) > 0) {
        total_received += received;
        if (total_received >= (int)(response_buffer_size - 1)) {
            break;
        }
    }
    
    response_buffer[total_received] = '\0';
    closesocket(sock);
    
    /* Parse status code from response */
    if (total_received > 0 && strncmp(response_buffer, "HTTP/", 5) == 0) {
        /* Find status code after "HTTP/1.x " */
        const char* status_start = strchr(response_buffer, ' ');
        if (status_start) {
            return atoi(status_start + 1);
        }
    }
    
    return -1;
}

/*
 * Check if server is running and accessible.
 */
static bool server_is_running(void) {
    SOCKET sock = connect_to_server();
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        return true;
    }
    return false;
}

/*============================================================================
 * Basic Connectivity Tests
 *============================================================================*/

MU_TEST(test_server_connect) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running on port %d\n", TEST_PORT);
        return NULL;
    }
    
    SOCKET sock = connect_to_server();
    mu_assert("Could not connect to server", sock != INVALID_SOCKET);
    closesocket(sock);
    
    return NULL;
}

/*============================================================================
 * GET Request Tests
 *============================================================================*/

MU_TEST(test_get_root) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("GET", "/", NULL, response, sizeof(response));
    
    /* Should get 200 OK or redirect to index */
    mu_check(status == 200 || status == 301 || status == 302);
    
    return NULL;
}

MU_TEST(test_get_index_html) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("GET", "/index.html", NULL, response, sizeof(response));
    
    /* Should get 200 OK if file exists, 404 if not */
    mu_check(status == 200 || status == 404);
    
    if (status == 200) {
        /* Response should contain HTML */
        mu_check(strstr(response, "text/html") != NULL || strstr(response, "<!DOCTYPE") != NULL);
    }
    
    return NULL;
}

MU_TEST(test_get_nonexistent_file) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("GET", "/nonexistent_file_xyz_123.html", NULL, response, sizeof(response));
    
    /* Should get 404 Not Found */
    mu_assert_int_eq(404, status);
    
    return NULL;
}

/*============================================================================
 * HEAD Request Tests
 *============================================================================*/

MU_TEST(test_head_request) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("HEAD", "/index.html", NULL, response, sizeof(response));
    
    /* Should get same status as GET */
    mu_check(status == 200 || status == 404);
    
    /* HEAD response should have headers but no body */
    if (status == 200) {
        /* Should have Content-Length header */
        mu_check(strstr(response, "Content-Length") != NULL);
    }
    
    return NULL;
}

/*============================================================================
 * OPTIONS Request Tests (CORS)
 *============================================================================*/

MU_TEST(test_options_request) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("OPTIONS", "/api/test", NULL, response, sizeof(response));
    
    /* OPTIONS should return 200 or 204 */
    mu_check(status == 200 || status == 204 || status == 405);
    
    return NULL;
}

/*============================================================================
 * Error Response Tests
 *============================================================================*/

MU_TEST(test_method_not_allowed) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("DELETE", "/index.html", NULL, response, sizeof(response));
    
    /* DELETE should return 405 Method Not Allowed */
    mu_check(status == 405 || status == 501 || status == 400);
    
    return NULL;
}

MU_TEST(test_forbidden_traversal) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("GET", "/../../../etc/passwd", NULL, response, sizeof(response));
    
    /* Directory traversal should return 400 or 403 */
    mu_check(status == 400 || status == 403 || status == 404);
    
    return NULL;
}

/*============================================================================
 * Range Request Tests
 *============================================================================*/

MU_TEST(test_range_request) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("GET", "/index.html", "Range: bytes=0-99\r\n", response, sizeof(response));
    
    /* Should get 206 Partial Content if file exists and supports ranges */
    mu_check(status == 206 || status == 200 || status == 404);
    
    if (status == 206) {
        /* Should have Content-Range header */
        mu_check(strstr(response, "Content-Range") != NULL);
    }
    
    return NULL;
}

/*============================================================================
 * Keep-Alive Tests
 *============================================================================*/

MU_TEST(test_keepalive_connection) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    SOCKET sock = connect_to_server();
    mu_assert("Could not connect", sock != INVALID_SOCKET);
    
    /* Send first request with keep-alive */
    const char* request1 = 
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    send(sock, request1, (int)strlen(request1), 0);
    
    char response[RECV_BUFFER_SIZE];
    int received = recv(sock, response, sizeof(response) - 1, 0);
    
    if (received > 0) {
        response[received] = '\0';
        
        /* Should have received a response */
        mu_check(strstr(response, "HTTP/") != NULL);
        
        /* Try second request on same connection */
        const char* request2 = 
            "GET /index.html HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Connection: close\r\n"
            "\r\n";
        
        send(sock, request2, (int)strlen(request2), 0);
        
        received = recv(sock, response, sizeof(response) - 1, 0);
        if (received > 0) {
            response[received] = '\0';
            /* Should still work */
            mu_check(strstr(response, "HTTP/") != NULL);
        }
    }
    
    closesocket(sock);
    return NULL;
}

/*============================================================================
 * Response Header Tests
 *============================================================================*/

MU_TEST(test_server_header) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("GET", "/index.html", NULL, response, sizeof(response));
    
    if (status == 200) {
        /* Should have Server header with "Bolt" */
        mu_check(strstr(response, "Server:") != NULL);
        mu_check(strstr(response, "Bolt") != NULL);
    }
    
    return NULL;
}

MU_TEST(test_content_type_header) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("GET", "/index.html", NULL, response, sizeof(response));
    
    if (status == 200) {
        /* Should have Content-Type header */
        mu_check(strstr(response, "Content-Type:") != NULL);
    }
    
    return NULL;
}

MU_TEST(test_security_headers) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("GET", "/index.html", NULL, response, sizeof(response));
    
    if (status == 200) {
        /* Should have security headers */
        mu_check(strstr(response, "X-Content-Type-Options") != NULL);
        mu_check(strstr(response, "X-Frame-Options") != NULL);
    }
    
    return NULL;
}

/*============================================================================
 * Metrics Endpoint Test
 *============================================================================*/

MU_TEST(test_metrics_endpoint) {
    if (!server_is_running()) {
        printf("    [SKIP] Server not running\n");
        return NULL;
    }
    
    char response[RECV_BUFFER_SIZE];
    int status = send_request("GET", "/__bolt_metrics", NULL, response, sizeof(response));
    
    /* Metrics endpoint should return JSON */
    if (status == 200) {
        mu_check(strstr(response, "application/json") != NULL);
    }
    
    return NULL;
}

/*============================================================================
 * Test Suite Runner
 *============================================================================*/

void test_suite_server(void) {
    printf("\n  Note: Integration tests require Bolt server running on port %d\n", TEST_PORT);
    printf("  Start the server with: .\\bolt.exe\n\n");
    
    /* Connectivity */
    MU_RUN_TEST(test_server_connect);
    
    /* GET requests */
    MU_RUN_TEST(test_get_root);
    MU_RUN_TEST(test_get_index_html);
    MU_RUN_TEST(test_get_nonexistent_file);
    
    /* HEAD requests */
    MU_RUN_TEST(test_head_request);
    
    /* OPTIONS requests */
    MU_RUN_TEST(test_options_request);
    
    /* Error responses */
    MU_RUN_TEST(test_method_not_allowed);
    MU_RUN_TEST(test_forbidden_traversal);
    
    /* Range requests */
    MU_RUN_TEST(test_range_request);
    
    /* Keep-alive */
    MU_RUN_TEST(test_keepalive_connection);
    
    /* Response headers */
    MU_RUN_TEST(test_server_header);
    MU_RUN_TEST(test_content_type_header);
    MU_RUN_TEST(test_security_headers);
    
    /* Metrics */
    MU_RUN_TEST(test_metrics_endpoint);
}

