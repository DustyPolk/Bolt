#ifndef TLS_H
#define TLS_H

#define SECURITY_WIN32
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <schannel.h>
#include <sspi.h>
#include <stdbool.h>
#include "bolt.h"

/*
 * TLS context for a connection.
 */
typedef struct BoltTLSContext {
    /* SChannel handles */
    CredHandle cred_handle;
    CtxtHandle context_handle;
    bool handshake_complete;
    SecPkgContext_StreamSizes stream_sizes;
    
    /* Buffers */
    char* read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_offset;
    
    char* write_buffer;
    size_t write_buffer_size;
} BoltTLSContext;

/*
 * TLS server configuration.
 */
typedef struct BoltTLSConfig {
    const char* cert_file;
    const char* key_file;
    bool enabled;
} BoltTLSConfig;

/*
 * Initialize TLS subsystem.
 */
bool tls_init(void);

/*
 * Cleanup TLS subsystem.
 */
void tls_cleanup(void);

/*
 * Load certificate and key from files.
 */
bool tls_load_certificate(const char* cert_file, const char* key_file);

/*
 * Create TLS context for a connection.
 */
BoltTLSContext* tls_create_context(SOCKET socket);

/*
 * Perform TLS handshake.
 */
bool tls_handshake(BoltTLSContext* tls, SOCKET socket);

/*
 * Encrypt data for sending.
 */
bool tls_encrypt(BoltTLSContext* tls, const char* plaintext, size_t plaintext_len,
                 char* ciphertext, size_t* ciphertext_len);

/*
 * Decrypt received data.
 */
bool tls_decrypt(BoltTLSContext* tls, const char* ciphertext, size_t ciphertext_len,
                 char* plaintext, size_t* plaintext_len);

/*
 * Destroy TLS context.
 */
void tls_destroy_context(BoltTLSContext* tls);

#endif /* TLS_H */

