#include "../include/mime.h"
#include <string.h>
#include <ctype.h>

/*
 * MIME type mapping structure.
 */
typedef struct {
    const char* extension;
    const char* mime_type;
} MimeEntry;

/*
 * MIME type lookup table (sorted for potential binary search).
 * Common web file types.
 */
static const MimeEntry mime_types[] = {
    /* Text */
    { "css",   "text/css" },
    { "csv",   "text/csv" },
    { "htm",   "text/html" },
    { "html",  "text/html" },
    { "js",    "text/javascript" },
    { "json",  "application/json" },
    { "mjs",   "text/javascript" },
    { "txt",   "text/plain" },
    { "xml",   "application/xml" },
    
    /* Images */
    { "avif",  "image/avif" },
    { "bmp",   "image/bmp" },
    { "gif",   "image/gif" },
    { "ico",   "image/x-icon" },
    { "jpeg",  "image/jpeg" },
    { "jpg",   "image/jpeg" },
    { "png",   "image/png" },
    { "svg",   "image/svg+xml" },
    { "webp",  "image/webp" },
    
    /* Fonts */
    { "eot",   "application/vnd.ms-fontobject" },
    { "otf",   "font/otf" },
    { "ttf",   "font/ttf" },
    { "woff",  "font/woff" },
    { "woff2", "font/woff2" },
    
    /* Audio/Video */
    { "mp3",   "audio/mpeg" },
    { "mp4",   "video/mp4" },
    { "ogg",   "audio/ogg" },
    { "wav",   "audio/wav" },
    { "webm",  "video/webm" },
    
    /* Documents */
    { "pdf",   "application/pdf" },
    { "zip",   "application/zip" },
    
    /* Web Assembly */
    { "wasm",  "application/wasm" },
    
    { NULL, NULL }  /* Sentinel */
};

/*
 * Text-based MIME types (for charset addition).
 */
static const char* text_types[] = {
    "text/",
    "application/json",
    "application/xml",
    "application/javascript",
    "image/svg+xml",
    NULL
};

/*
 * Get the MIME type for a given file extension.
 */
const char* mime_get_type(const char* extension) {
    if (!extension || !*extension) {
        return "application/octet-stream";
    }
    
    /* Case-insensitive comparison */
    char lower_ext[32];
    size_t i;
    for (i = 0; i < sizeof(lower_ext) - 1 && extension[i]; i++) {
        lower_ext[i] = (char)tolower((unsigned char)extension[i]);
    }
    lower_ext[i] = '\0';
    
    /* Linear search (fast enough for this table size) */
    for (const MimeEntry* entry = mime_types; entry->extension; entry++) {
        if (strcmp(lower_ext, entry->extension) == 0) {
            return entry->mime_type;
        }
    }
    
    return "application/octet-stream";
}

/*
 * Check if a MIME type is text-based.
 */
int mime_is_text(const char* mime_type) {
    if (!mime_type) {
        return 0;
    }
    
    for (const char** type = text_types; *type; type++) {
        size_t len = strlen(*type);
        if (strncmp(mime_type, *type, len) == 0) {
            return 1;
        }
    }
    
    return 0;
}

