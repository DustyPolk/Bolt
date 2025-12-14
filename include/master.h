#ifndef MASTER_H
#define MASTER_H

#include "bolt.h"
#include <stdbool.h>

/*
 * Master/Worker process model.
 */

/*
 * Run as master process (manages workers).
 */
bool master_run(int argc, char* argv[], int worker_count);

/*
 * Run as worker process.
 */
bool worker_run(int argc, char* argv[]);

/*
 * Check if current process is master.
 */
bool master_is_master(void);

#endif /* MASTER_H */

