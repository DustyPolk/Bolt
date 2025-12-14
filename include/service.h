#ifndef SERVICE_H
#define SERVICE_H

#include "bolt.h"
#include <stdbool.h>

/*
 * Windows Service support.
 */

/*
 * Install Bolt as a Windows Service.
 */
bool service_install(const char* service_name, const char* display_name,
                     const char* description, const char* config_path);

/*
 * Uninstall Bolt Windows Service.
 */
bool service_uninstall(const char* service_name);

/*
 * Run Bolt as a Windows Service.
 */
bool service_run(const char* service_name, int argc, char* argv[]);

/*
 * Check if running as a service.
 */
bool service_is_running(void);

#endif /* SERVICE_H */

