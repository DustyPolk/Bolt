#include "../include/vhost.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Create virtual host manager.
 */
BoltVHostManager* vhost_manager_create(void) {
    BoltVHostManager* manager = (BoltVHostManager*)calloc(1, sizeof(BoltVHostManager));
    return manager;
}

/*
 * Destroy virtual host manager.
 */
void vhost_manager_destroy(BoltVHostManager* manager) {
    if (!manager) return;
    
    BoltVHost* vhost = manager->vhosts;
    while (vhost) {
        BoltVHost* next = vhost->next;
        free(vhost);
        vhost = next;
    }
    
    free(manager);
}

/*
 * Add a virtual host.
 */
bool vhost_add(BoltVHostManager* manager, const char* server_name, const char* root,
               const char* index_file, const char* access_log, const char* error_log,
               bool enable_dir_listing) {
    if (!manager) return false;
    
    BoltVHost* vhost = (BoltVHost*)calloc(1, sizeof(BoltVHost));
    if (!vhost) return false;
    
    if (server_name) {
        strncpy(vhost->server_name, server_name, sizeof(vhost->server_name) - 1);
    }
    if (root) {
        strncpy(vhost->root, root, sizeof(vhost->root) - 1);
    }
    if (index_file) {
        strncpy(vhost->index_file, index_file, sizeof(vhost->index_file) - 1);
    }
    if (access_log) {
        strncpy(vhost->access_log, access_log, sizeof(vhost->access_log) - 1);
    }
    if (error_log) {
        strncpy(vhost->error_log, error_log, sizeof(vhost->error_log) - 1);
    }
    vhost->enable_dir_listing = enable_dir_listing;
    
    /* Add to list */
    vhost->next = manager->vhosts;
    manager->vhosts = vhost;
    
    /* Set as default if no default exists */
    if (!manager->default_vhost) {
        manager->default_vhost = vhost;
    }
    
    return true;
}

/*
 * Find virtual host by Host header.
 */
BoltVHost* vhost_find(BoltVHostManager* manager, const char* host_header) {
    if (!manager || !host_header || !host_header[0]) {
        return manager ? manager->default_vhost : NULL;
    }
    
    /* Remove port if present */
    char host[256];
    strncpy(host, host_header, sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';
    
    char* colon = strchr(host, ':');
    if (colon) {
        *colon = '\0';
    }
    
    /* Search for matching server name */
    BoltVHost* vhost = manager->vhosts;
    while (vhost) {
        if (strcmp(vhost->server_name, host) == 0) {
            return vhost;
        }
        vhost = vhost->next;
    }
    
    /* Return default if no match */
    return manager->default_vhost;
}

/*
 * Get default virtual host.
 */
BoltVHost* vhost_get_default(BoltVHostManager* manager) {
    return manager ? manager->default_vhost : NULL;
}

