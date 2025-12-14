#ifndef VHOST_H
#define VHOST_H

#include "bolt.h"
#include "config.h"
#include <stdbool.h>

/*
 * Virtual host configuration.
 */
typedef struct BoltVHost {
    char server_name[256];
    char root[512];
    char index_file[64];
    char access_log[512];
    char error_log[512];
    bool enable_dir_listing;
    struct BoltVHost* next;
} BoltVHost;

/*
 * Virtual host manager.
 */
typedef struct BoltVHostManager {
    BoltVHost* vhosts;
    BoltVHost* default_vhost;
} BoltVHostManager;

/*
 * Create virtual host manager.
 */
BoltVHostManager* vhost_manager_create(void);

/*
 * Destroy virtual host manager.
 */
void vhost_manager_destroy(BoltVHostManager* manager);

/*
 * Add a virtual host.
 */
bool vhost_add(BoltVHostManager* manager, const char* server_name, const char* root,
               const char* index_file, const char* access_log, const char* error_log,
               bool enable_dir_listing);

/*
 * Find virtual host by Host header.
 */
BoltVHost* vhost_find(BoltVHostManager* manager, const char* host_header);

/*
 * Get default virtual host.
 */
BoltVHost* vhost_get_default(BoltVHostManager* manager);

#endif /* VHOST_H */

