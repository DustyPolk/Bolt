#include "../include/iocp.h"
#include "../include/connection.h"
#include <stdio.h>
#include <stdlib.h>

/* GUID for loading extension functions */
static GUID GuidAcceptEx = WSAID_ACCEPTEX;
static GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
static GUID GuidTransmitFile = WSAID_TRANSMITFILE;
static GUID GuidDisconnectEx = WSAID_DISCONNECTEX;

/*
 * Load Winsock extension function.
 */
static void* load_extension_function(SOCKET socket, GUID* guid) {
    void* func = NULL;
    DWORD bytes = 0;
    
    WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
             guid, sizeof(GUID),
             &func, sizeof(func),
             &bytes, NULL, NULL);
    
    return func;
}

/*
 * Create IOCP subsystem.
 */
BoltIOCP* bolt_iocp_create(int port, int num_accept_posts) {
    WSADATA wsa_data;
    struct sockaddr_in addr;
    
    /* Initialize Winsock */
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        BOLT_ERROR("WSAStartup failed: %d", WSAGetLastError());
        return NULL;
    }
    
    BoltIOCP* iocp = (BoltIOCP*)calloc(1, sizeof(BoltIOCP));
    if (!iocp) {
        WSACleanup();
        return NULL;
    }
    
    /* Create IOCP */
    iocp->handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!iocp->handle) {
        BOLT_ERROR("CreateIoCompletionPort failed: %lu", (unsigned long)GetLastError());
        free(iocp);
        WSACleanup();
        return NULL;
    }
    
    /* Create listening socket */
    iocp->listen_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                      NULL, 0, WSA_FLAG_OVERLAPPED);
    if (iocp->listen_socket == INVALID_SOCKET) {
        BOLT_ERROR("WSASocket failed: %d", WSAGetLastError());
        CloseHandle(iocp->handle);
        free(iocp);
        WSACleanup();
        return NULL;
    }
    
    /* Load extension functions */
    iocp->AcceptEx = (LPFN_ACCEPTEX)load_extension_function(
        iocp->listen_socket, &GuidAcceptEx);
    iocp->GetAcceptExSockaddrs = (LPFN_GETACCEPTEXSOCKADDRS)load_extension_function(
        iocp->listen_socket, &GuidGetAcceptExSockaddrs);
    iocp->TransmitFile = (LPFN_TRANSMITFILE)load_extension_function(
        iocp->listen_socket, &GuidTransmitFile);
    iocp->DisconnectEx = (LPFN_DISCONNECTEX)load_extension_function(
        iocp->listen_socket, &GuidDisconnectEx);
    
    if (!iocp->AcceptEx || !iocp->TransmitFile) {
        BOLT_ERROR("Failed to load Winsock extensions");
        closesocket(iocp->listen_socket);
        CloseHandle(iocp->handle);
        free(iocp);
        WSACleanup();
        return NULL;
    }
    
    /* Set socket options */
    int opt = 1;
    setsockopt(iocp->listen_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    setsockopt(iocp->listen_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));
    
    /* Bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);
    
    if (bind(iocp->listen_socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        BOLT_ERROR("Bind failed: %d (port %d may be in use)", WSAGetLastError(), port);
        closesocket(iocp->listen_socket);
        CloseHandle(iocp->handle);
        free(iocp);
        WSACleanup();
        return NULL;
    }
    
    /* Listen */
    if (listen(iocp->listen_socket, BOLT_BACKLOG) == SOCKET_ERROR) {
        BOLT_ERROR("Listen failed: %d", WSAGetLastError());
        closesocket(iocp->listen_socket);
        CloseHandle(iocp->handle);
        free(iocp);
        WSACleanup();
        return NULL;
    }
    
    /* Associate listen socket with IOCP */
    if (!CreateIoCompletionPort((HANDLE)iocp->listen_socket, iocp->handle, 0, 0)) {
        BOLT_ERROR("Failed to associate listen socket with IOCP");
        closesocket(iocp->listen_socket);
        CloseHandle(iocp->handle);
        free(iocp);
        WSACleanup();
        return NULL;
    }
    
    /* Pre-allocate accept structures */
    iocp->num_accepts = num_accept_posts;
    iocp->accept_overlaps = (BoltOverlapped*)calloc(num_accept_posts, sizeof(BoltOverlapped));
    iocp->accept_sockets = (SOCKET*)malloc(num_accept_posts * sizeof(SOCKET));
    
    if (!iocp->accept_overlaps || !iocp->accept_sockets) {
        BOLT_ERROR("Failed to allocate accept structures");
        bolt_iocp_destroy(iocp);
        return NULL;
    }
    
    /* Initialize accept sockets */
    for (int i = 0; i < num_accept_posts; i++) {
        iocp->accept_sockets[i] = INVALID_SOCKET;
        iocp->accept_overlaps[i].op_type = BOLT_OP_ACCEPT;
        iocp->accept_overlaps[i].accept_index = i;
    }
    
    /* Post initial accepts */
    for (int i = 0; i < num_accept_posts; i++) {
        if (!bolt_iocp_post_accept(iocp, i)) {
            BOLT_ERROR("Failed to post initial accept %d", i);
        }
    }
    
    iocp->running = true;
    return iocp;
}

/*
 * Destroy IOCP.
 */
void bolt_iocp_destroy(BoltIOCP* iocp) {
    if (!iocp) return;
    
    iocp->running = false;
    
    /* Close accept sockets */
    if (iocp->accept_sockets) {
        for (int i = 0; i < iocp->num_accepts; i++) {
            if (iocp->accept_sockets[i] != INVALID_SOCKET) {
                closesocket(iocp->accept_sockets[i]);
            }
        }
        free(iocp->accept_sockets);
    }
    
    free(iocp->accept_overlaps);
    
    if (iocp->listen_socket != INVALID_SOCKET) {
        closesocket(iocp->listen_socket);
    }
    
    if (iocp->handle) {
        CloseHandle(iocp->handle);
    }
    
    WSACleanup();
    free(iocp);
}

/*
 * Associate socket with IOCP.
 */
bool bolt_iocp_associate(BoltIOCP* iocp, SOCKET socket, void* completion_key) {
    HANDLE result = CreateIoCompletionPort((HANDLE)socket, iocp->handle,
                                           (ULONG_PTR)completion_key, 0);
    return result != NULL;
}

/*
 * Post an AcceptEx operation.
 */
bool bolt_iocp_post_accept(BoltIOCP* iocp, int accept_index) {
    if (accept_index < 0 || accept_index >= iocp->num_accepts) {
        return false;
    }
    
    /* Create accept socket */
    SOCKET accept_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                       NULL, 0, WSA_FLAG_OVERLAPPED);
    if (accept_socket == INVALID_SOCKET) {
        BOLT_ERROR("Failed to create accept socket: %d", WSAGetLastError());
        return false;
    }
    
    iocp->accept_sockets[accept_index] = accept_socket;
    
    /* Setup overlapped */
    BoltOverlapped* overlap = &iocp->accept_overlaps[accept_index];
    memset(&overlap->overlapped, 0, sizeof(OVERLAPPED));
    overlap->op_type = BOLT_OP_ACCEPT;
    overlap->accept_index = accept_index;
    
    /* Post AcceptEx */
    DWORD bytes_received = 0;
    BOOL result = iocp->AcceptEx(
        iocp->listen_socket,
        accept_socket,
        overlap->buffer,
        BOLT_ACCEPT_RECV_BYTES,  /* Receive small initial data to reduce syscalls */
        sizeof(struct sockaddr_in) + 16,
        sizeof(struct sockaddr_in) + 16,
        &bytes_received,
        &overlap->overlapped
    );
    
    if (!result && WSAGetLastError() != ERROR_IO_PENDING) {
        BOLT_ERROR("AcceptEx failed: %d", WSAGetLastError());
        closesocket(accept_socket);
        iocp->accept_sockets[accept_index] = INVALID_SOCKET;
        return false;
    }
    
    return true;
}

/*
 * Post a receive operation.
 */
bool bolt_iocp_post_recv(BoltIOCP* iocp, BoltConnection* conn) {
    BOLT_UNUSED(iocp);
    if (!conn) return false;
    
    BoltOverlapped* overlap = &conn->recv_overlapped;
    memset(&overlap->overlapped, 0, sizeof(OVERLAPPED));
    overlap->op_type = BOLT_OP_RECV;
    overlap->connection = conn;
    overlap->wsa_buf.buf = conn->recv_buffer + conn->recv_offset;
    overlap->wsa_buf.len = (ULONG)(conn->recv_buffer_size - conn->recv_offset);
    
    DWORD flags = 0;
    DWORD bytes_received = 0;
    
    int result = WSARecv(conn->socket, &overlap->wsa_buf, 1,
                         &bytes_received, &flags,
                         &overlap->overlapped, NULL);
    
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        return false;
    }
    
    return true;
}

/*
 * Post a send operation.
 */
bool bolt_iocp_post_send(BoltIOCP* iocp, BoltConnection* conn,
                         const char* data, size_t len) {
    BOLT_UNUSED(iocp);
    if (!conn || !data || len == 0) return false;
    
    /* Copy data to send buffer if needed */
    if (data != conn->send_buffer) {
        if (len > conn->send_buffer_size) {
            return false;
        }
        memcpy(conn->send_buffer, data, len);
    }
    
    conn->send_remaining = len;
    conn->send_offset = 0;
    
    BoltOverlapped* overlap = &conn->send_overlapped;
    memset(&overlap->overlapped, 0, sizeof(OVERLAPPED));
    overlap->op_type = BOLT_OP_SEND;
    overlap->connection = conn;
    overlap->wsa_buf.buf = conn->send_buffer;
    overlap->wsa_buf.len = (ULONG)len;
    
    DWORD bytes_sent = 0;
    
    int result = WSASend(conn->socket, &overlap->wsa_buf, 1,
                         &bytes_sent, 0,
                         &overlap->overlapped, NULL);
    
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        return false;
    }
    
    return true;
}

/*
 * Post TransmitFile (zero-copy).
 */
bool bolt_iocp_post_transmit_file(BoltIOCP* iocp, BoltConnection* conn,
                                   HANDLE file, size_t file_size,
                                   const char* headers, size_t header_len) {
    if (!conn || file == INVALID_HANDLE_VALUE) return false;
    
    conn->file_handle = file;
    conn->file_size = file_size;
    conn->file_offset = 0;
    
    BoltOverlapped* overlap = &conn->send_overlapped;
    memset(&overlap->overlapped, 0, sizeof(OVERLAPPED));
    overlap->op_type = BOLT_OP_TRANSMIT_FILE;
    overlap->connection = conn;
    
    /* Setup transmit buffers for headers */
    TRANSMIT_FILE_BUFFERS tfb = {0};
    if (headers && header_len > 0) {
        /* Copy headers to connection buffer */
        memcpy(conn->send_buffer, headers, header_len);
        tfb.Head = conn->send_buffer;
        tfb.HeadLength = (DWORD)header_len;
    }
    
    /* Use TF_USE_KERNEL_APC for best performance */
    DWORD flags = TF_USE_KERNEL_APC;
    if (conn->keep_alive) {
        flags |= TF_REUSE_SOCKET;
    }
    
    BOOL result = iocp->TransmitFile(
        conn->socket,
        file,
        (DWORD)file_size,
        0,  /* Use default send size */
        &overlap->overlapped,
        headers ? &tfb : NULL,
        flags
    );
    
    if (!result && WSAGetLastError() != WSA_IO_PENDING) {
        BOLT_ERROR("TransmitFile failed: %d", WSAGetLastError());
        return false;
    }
    
    return true;
}

/*
 * Post disconnect for reuse.
 */
bool bolt_iocp_post_disconnect(BoltIOCP* iocp, BoltConnection* conn) {
    if (!conn || !iocp->DisconnectEx) return false;
    
    BoltOverlapped* overlap = &conn->send_overlapped;
    memset(&overlap->overlapped, 0, sizeof(OVERLAPPED));
    overlap->op_type = BOLT_OP_DISCONNECT;
    overlap->connection = conn;
    
    BOOL result = iocp->DisconnectEx(conn->socket, &overlap->overlapped,
                                      TF_REUSE_SOCKET, 0);
    
    if (!result && WSAGetLastError() != WSA_IO_PENDING) {
        return false;
    }
    
    return true;
}

/*
 * Get completion from IOCP.
 */
bool bolt_iocp_get_completion(BoltIOCP* iocp,
                               DWORD* bytes_transferred,
                               ULONG_PTR* completion_key,
                               BoltOverlapped** overlapped,
                               DWORD timeout_ms) {
    OVERLAPPED* ovl = NULL;
    
    BOOL result = GetQueuedCompletionStatus(
        iocp->handle,
        bytes_transferred,
        completion_key,
        &ovl,
        timeout_ms
    );
    
    *overlapped = (BoltOverlapped*)ovl;
    
    if (!result) {
        if (ovl == NULL) {
            /* Timeout or error with no overlapped */
            return false;
        }
        /* Error with overlapped - still return it for cleanup */
    }
    
    return true;
}

