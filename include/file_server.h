#ifndef FILE_SERVER_H
#define FILE_SERVER_H

#include <winsock2.h>
#include "http.h"

/* Forward declaration (Bolt async path) */
typedef struct BoltConnection BoltConnection;

/*
 * Static file server with directory listing support.
 */

/*
 * Handle a file serving request.
 * Takes a parsed HTTP request and serves the appropriate response.
 */
void file_server_handle(SOCKET client, const HttpRequest* request);

/*
 * Serve a specific file.
 * Returns true on success, false on failure.
 */
bool file_server_serve_file(SOCKET client, const char* filepath, 
                            const HttpRequest* request);

/*
 * Generate and serve a directory listing.
 */
void file_server_serve_directory(SOCKET client, const char* dirpath,
                                 const char* uri);

/*
 * Bolt async fast-path:
 * Builds a response and posts async send/transmit operations via IOCP.
 * Does NOT block on disk I/O for cached small assets.
 */
void bolt_file_server_handle(BoltConnection* conn, const HttpRequest* request);

#endif /* FILE_SERVER_H */

