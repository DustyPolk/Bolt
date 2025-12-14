#ifndef IOCP_H
#define IOCP_H

#include "bolt.h"
#include <mswsock.h>

/*
 * IOCP (I/O Completion Port) wrapper for high-performance async I/O.
 */

/* Extended overlapped structure for tracking operations */
typedef struct BoltOverlapped {
    OVERLAPPED overlapped;      /* Must be first! */
    BoltOperationType op_type;
    int accept_index;           /* O(1) mapping for AcceptEx completions */
    BoltConnection* connection;
    WSABUF wsa_buf;
    char buffer[BOLT_ACCEPT_BUFFER_SIZE];
} BoltOverlapped;

/* IOCP context */
typedef struct BoltIOCP {
    HANDLE handle;              /* IOCP handle */
    SOCKET listen_socket;       /* Listening socket */
    
    /* AcceptEx function pointer (loaded dynamically) */
    LPFN_ACCEPTEX AcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrs;
    LPFN_TRANSMITFILE TransmitFile;
    LPFN_DISCONNECTEX DisconnectEx;
    
    /* Pre-posted accepts for high connection rate */
    BoltOverlapped* accept_overlaps;
    SOCKET* accept_sockets;
    int num_accepts;
    
    volatile bool running;
} BoltIOCP;

/*
 * Initialize IOCP subsystem.
 * Returns IOCP handle on success, NULL on failure.
 */
BoltIOCP* bolt_iocp_create(int port, int num_accept_posts);

/*
 * Destroy IOCP and cleanup.
 */
void bolt_iocp_destroy(BoltIOCP* iocp);

/*
 * Associate a socket with the IOCP.
 */
bool bolt_iocp_associate(BoltIOCP* iocp, SOCKET socket, void* completion_key);

/*
 * Post an accept operation.
 */
bool bolt_iocp_post_accept(BoltIOCP* iocp, int accept_index);

/*
 * Post a receive operation.
 */
bool bolt_iocp_post_recv(BoltIOCP* iocp, BoltConnection* conn);

/*
 * Post a send operation.
 */
bool bolt_iocp_post_send(BoltIOCP* iocp, BoltConnection* conn, 
                         const char* data, size_t len);

/*
 * Post a TransmitFile operation (zero-copy).
 * If range_start and range_length are non-zero, only that range is sent.
 */
bool bolt_iocp_post_transmit_file(BoltIOCP* iocp, BoltConnection* conn,
                                   HANDLE file, size_t file_size,
                                   const char* headers, size_t header_len,
                                   size_t range_start, size_t range_length);

/*
 * Post disconnect for connection reuse.
 */
bool bolt_iocp_post_disconnect(BoltIOCP* iocp, BoltConnection* conn);

/*
 * Get completion status (called by worker threads).
 */
bool bolt_iocp_get_completion(BoltIOCP* iocp, 
                               DWORD* bytes_transferred,
                               ULONG_PTR* completion_key,
                               BoltOverlapped** overlapped,
                               DWORD timeout_ms);

#endif /* IOCP_H */

