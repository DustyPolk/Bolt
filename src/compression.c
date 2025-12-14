#include "../include/compression.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Try to include zlib - if available via MinGW package manager */
#if __has_include(<zlib.h>)
#include <zlib.h>
#define HAVE_ZLIB 1
#elif defined(HAVE_ZLIB_H)
#include <zlib.h>
#define HAVE_ZLIB 1
#else
/* zlib not found - compression will be disabled */
#define HAVE_ZLIB 0
#endif

/*
 * Get default compression config.
 */
BoltCompressionConfig compression_get_default_config(void) {
    BoltCompressionConfig config = {
        .enabled = true,
        .level = 6,  /* Good balance between speed and compression */
        .min_size = 256,  /* Don't compress very small files */
        .preferred_type = BOLT_COMPRESS_GZIP
    };
    return config;
}

/*
 * Parse Accept-Encoding header and determine best compression type.
 */
BoltCompressionType compression_parse_accept_encoding(const char* accept_encoding,
                                                       const BoltCompressionConfig* config) {
    if (!accept_encoding || !config || !config->enabled) {
        return BOLT_COMPRESS_NONE;
    }
    
#if !HAVE_ZLIB
    /* Compression not available if zlib not found */
    return BOLT_COMPRESS_NONE;
#endif
    
    /* Case-insensitive search for supported encodings */
    char lower[256];
    size_t len = strlen(accept_encoding);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    
    for (size_t i = 0; i < len; i++) {
        lower[i] = (char)tolower((unsigned char)accept_encoding[i]);
    }
    lower[len] = '\0';
    
    /* Check for gzip (preferred) */
    if (strstr(lower, "gzip") != NULL) {
        return BOLT_COMPRESS_GZIP;
    }
    
    /* Check for deflate */
    if (strstr(lower, "deflate") != NULL) {
        return BOLT_COMPRESS_DEFLATE;
    }
    
    return BOLT_COMPRESS_NONE;
}

/*
 * Check if a content type should be compressed.
 */
bool compression_should_compress(const char* content_type, const BoltCompressionConfig* config) {
    if (!content_type || !config || !config->enabled) {
        return false;
    }
    
    /* Compress text-based content types */
    if (strncmp(content_type, "text/", 5) == 0) {
        return true;
    }
    
    if (strncmp(content_type, "application/javascript", 21) == 0 ||
        strncmp(content_type, "application/json", 16) == 0 ||
        strncmp(content_type, "application/xml", 15) == 0 ||
        strncmp(content_type, "application/xhtml+xml", 20) == 0) {
        return true;
    }
    
    /* Don't compress already compressed formats */
    if (strstr(content_type, "gzip") != NULL ||
        strstr(content_type, "compress") != NULL ||
        strstr(content_type, "deflate") != NULL ||
        strstr(content_type, "br") != NULL) {
        return false;
    }
    
    /* Don't compress images, videos, etc. */
    if (strncmp(content_type, "image/", 6) == 0 ||
        strncmp(content_type, "video/", 6) == 0 ||
        strncmp(content_type, "audio/", 6) == 0 ||
        strncmp(content_type, "application/zip", 15) == 0 ||
        strncmp(content_type, "application/x-", 14) == 0) {
        return false;
    }
    
    return false;
}

#if HAVE_ZLIB
/*
 * Compress data using gzip (zlib implementation).
 */
bool compression_gzip(const char* input, size_t input_size,
                      BoltCompressedData* compressed_data,
                      int level) {
    if (!input || !compressed_data || input_size == 0) {
        return false;
    }
    
    /* Validate compression level */
    if (level < 1) level = 1;
    if (level > 9) level = 9;
    
    /* Estimate compressed size (usually smaller, but allocate more for safety) */
    size_t dest_size = input_size + (input_size / 10) + 12;
    char* dest = (char*)malloc(dest_size);
    if (!dest) {
        return false;
    }
    
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    
    /* Initialize zlib with gzip format */
    if (deflateInit2(&stream, level, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        free(dest);
        return false;
    }
    
    stream.next_in = (Bytef*)input;
    stream.avail_in = (uInt)input_size;
    stream.next_out = (Bytef*)dest;
    stream.avail_out = (uInt)dest_size;
    
    /* Compress */
    int ret = deflate(&stream, Z_FINISH);
    
    if (ret == Z_STREAM_END) {
        /* Success - resize buffer to actual size */
        size_t compressed_size = dest_size - stream.avail_out;
        char* resized = (char*)realloc(dest, compressed_size);
        if (resized) {
            compressed_data->data = resized;
            compressed_data->size = compressed_size;
            compressed_data->type = BOLT_COMPRESS_GZIP;
            deflateEnd(&stream);
            return true;
        }
    }
    
    deflateEnd(&stream);
    free(dest);
    return false;
}

#else
/*
 * Compression not available - zlib not found.
 * Install zlib via MinGW package manager: pacman -S mingw-w64-x86_64-zlib
 */
bool compression_gzip(const char* input, size_t input_size,
                      BoltCompressedData* compressed_data,
                      int level) {
    (void)input;
    (void)input_size;
    (void)compressed_data;
    (void)level;
    return false;
}
#endif

