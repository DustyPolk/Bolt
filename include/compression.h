#ifndef COMPRESSION_H
#define COMPRESSION_H

#include "bolt.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Compression types supported.
 */
typedef enum {
    BOLT_COMPRESS_NONE = 0,
    BOLT_COMPRESS_GZIP = 1,
    BOLT_COMPRESS_DEFLATE = 2
} BoltCompressionType;

/*
 * Compression configuration.
 */
typedef struct {
    bool enabled;
    int level;  /* 1-9, 6 is default */
    size_t min_size;  /* Minimum file size to compress (default: 256 bytes) */
    BoltCompressionType preferred_type;
} BoltCompressionConfig;

/*
 * Compressed data buffer.
 */
typedef struct {
    char* data;
    size_t size;
    BoltCompressionType type;
} BoltCompressedData;

/*
 * Parse Accept-Encoding header and determine best compression type.
 * Returns BOLT_COMPRESS_NONE if client doesn't support compression or
 * compression is disabled.
 */
BoltCompressionType compression_parse_accept_encoding(const char* accept_encoding,
                                                       const BoltCompressionConfig* config);

/*
 * Compress data using gzip.
 * Returns true on success, false on failure.
 * Caller must free compressed_data->data using free().
 */
bool compression_gzip(const char* input, size_t input_size,
                      BoltCompressedData* compressed_data,
                      int level);

/*
 * Check if a content type should be compressed.
 */
bool compression_should_compress(const char* content_type, const BoltCompressionConfig* config);

/*
 * Get default compression config.
 */
BoltCompressionConfig compression_get_default_config(void);

#endif /* COMPRESSION_H */

