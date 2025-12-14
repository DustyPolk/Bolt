#include "../include/file_sender.h"
#include "../include/bolt_server.h"
#include "../include/iocp.h"
#include <stdio.h>
#include <string.h>

/*
 * Open file for TransmitFile.
 */
HANDLE bolt_open_file(const char* filepath, size_t* out_size) {
    if (!filepath) return INVALID_HANDLE_VALUE;
    
    /* Open file with optimal flags for TransmitFile */
    HANDLE file = CreateFileA(
        filepath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );
    
    if (file == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }
    
    /* Get file size */
    if (out_size) {
        LARGE_INTEGER size;
        if (GetFileSizeEx(file, &size)) {
            *out_size = (size_t)size.QuadPart;
        } else {
            *out_size = 0;
        }
    }
    
    return file;
}

/*
 * Send file using TransmitFile.
 */
bool bolt_send_file(BoltConnection* conn, const char* filepath,
                    const char* headers, size_t header_len) {
    if (!conn || !filepath || !g_bolt_server) return false;
    
    size_t file_size = 0;
    HANDLE file = bolt_open_file(filepath, &file_size);
    
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    /* Store file handle in connection for cleanup */
    conn->file_handle = file;
    conn->file_size = file_size;
    
    /* Post TransmitFile operation */
    bool result = bolt_iocp_post_transmit_file(
        g_bolt_server->iocp,
        conn,
        file,
        file_size,
        headers,
        header_len
    );
    
    if (!result) {
        CloseHandle(file);
        conn->file_handle = INVALID_HANDLE_VALUE;
        return false;
    }
    
    conn->state = BOLT_CONN_SENDING_FILE;
    return true;
}

/*
 * Send response with headers and body.
 */
bool bolt_send_response(BoltConnection* conn,
                        const char* headers, size_t header_len,
                        const char* body, size_t body_len) {
    if (!conn || !g_bolt_server) return false;
    
    /* Calculate total size */
    size_t total_len = header_len + body_len;
    if (total_len > conn->send_buffer_size) {
        return false;  /* Too large */
    }
    
    /* Copy to send buffer */
    if (headers && header_len > 0) {
        memcpy(conn->send_buffer, headers, header_len);
    }
    if (body && body_len > 0) {
        memcpy(conn->send_buffer + header_len, body, body_len);
    }
    
    /* Post send operation */
    bool result = bolt_iocp_post_send(
        g_bolt_server->iocp,
        conn,
        conn->send_buffer,
        total_len
    );
    
    if (result) {
        conn->state = BOLT_CONN_SENDING;
    }
    
    return result;
}

/*
 * Send headers only.
 */
bool bolt_send_headers_only(BoltConnection* conn,
                            const char* headers, size_t header_len) {
    return bolt_send_response(conn, headers, header_len, NULL, 0);
}

