#ifndef REWRITE_H
#define REWRITE_H

#include "bolt.h"
#include <stdbool.h>

/*
 * Rewrite rule types.
 */
typedef enum {
    REWRITE_INTERNAL,   /* Internal redirect */
    REWRITE_REDIRECT_301,  /* Permanent redirect */
    REWRITE_REDIRECT_302   /* Temporary redirect */
} RewriteType;

/*
 * Rewrite rule.
 */
typedef struct BoltRewriteRule {
    char pattern[256];      /* Regex pattern (simplified - supports * wildcards) */
    char replacement[512];  /* Replacement string */
    RewriteType type;
    struct BoltRewriteRule* next;
} BoltRewriteRule;

/*
 * Rewrite engine.
 */
typedef struct BoltRewriteEngine {
    BoltRewriteRule* rules;
} BoltRewriteEngine;

/*
 * Create rewrite engine.
 */
BoltRewriteEngine* rewrite_engine_create(void);

/*
 * Destroy rewrite engine.
 */
void rewrite_engine_destroy(BoltRewriteEngine* engine);

/*
 * Add a rewrite rule.
 */
bool rewrite_add_rule(BoltRewriteEngine* engine, const char* pattern,
                      const char* replacement, RewriteType type);

/*
 * Apply rewrite rules to a URI.
 * Returns true if rewrite occurred, false otherwise.
 * Rewritten URI is written to out_uri (must be at least 2048 bytes).
 */
bool rewrite_apply(BoltRewriteEngine* engine, const char* uri, char* out_uri, size_t out_size);

/*
 * Match a pattern against a string (supports * wildcards).
 */
bool rewrite_match_pattern(const char* pattern, const char* str);

#endif /* REWRITE_H */

