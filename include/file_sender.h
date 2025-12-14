#ifndef FILE_SENDER_H
#define FILE_SENDER_H

#include "bolt.h"
#include "connection.h"
#include <windows.h>

/*
 * High-performance file sender using TransmitFile (zero-copy).
 */

/* File send context */
typedef struct {
    HANDLE file_handle;
    size_t file_size;
    char* headers;
    size_t header_len;
    bool keep_alive;
} BoltFileSend;

/*
 * Send a file using TransmitFile (zero-copy).
 * Returns true if transmission started, false on error.
 * The file is sent asynchronously - completion handled via IOCP.
 */
bool bolt_send_file(BoltConnection* conn, const char* filepath,
                    const char* headers, size_t header_len);

/*
 * Send headers and body using regular send.
 * Used for small responses or when TransmitFile isn't appropriate.
 */
bool bolt_send_response(BoltConnection* conn, 
                        const char* headers, size_t header_len,
                        const char* body, size_t body_len);

/*
 * Send just headers (for HEAD requests or 304 responses).
 */
bool bolt_send_headers_only(BoltConnection* conn,
                            const char* headers, size_t header_len);

/*
 * Open file and get size for TransmitFile.
 */
HANDLE bolt_open_file(const char* filepath, size_t* out_size);

#endif /* FILE_SENDER_H */

