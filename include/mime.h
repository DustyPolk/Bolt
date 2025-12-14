#ifndef MIME_H
#define MIME_H

/*
 * MIME type detection based on file extensions.
 */

/*
 * Get the MIME type for a given file extension.
 * Extension should not include the dot (e.g., "html" not ".html").
 * Returns "application/octet-stream" for unknown types.
 */
const char* mime_get_type(const char* extension);

/*
 * Check if a MIME type is text-based (for charset addition).
 */
int mime_is_text(const char* mime_type);

#endif /* MIME_H */

