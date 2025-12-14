#ifndef METRICS_H
#define METRICS_H

#include "bolt.h"
#include "bolt_server.h"
#include <stdbool.h>

/*
 * Generate JSON metrics response.
 * Returns true on success, false on failure.
 */
bool metrics_generate_json(BoltServer* server, char* buffer, size_t buffer_size, size_t* out_len);

/*
 * Check if URI is a metrics endpoint.
 */
bool metrics_is_endpoint(const char* uri);

#endif /* METRICS_H */

