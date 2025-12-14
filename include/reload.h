#ifndef RELOAD_H
#define RELOAD_H

#include "bolt.h"
#include "config.h"
#include <stdbool.h>

/*
 * Reload configuration without restarting server.
 * Returns true on success, false on failure.
 */
bool reload_config(BoltServer* server, const char* config_path);

/*
 * Setup reload signal handler.
 */
void reload_setup_signal_handler(BoltServer* server);

#endif /* RELOAD_H */

