#include "../include/rewrite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Simple wildcard matching (supports * only).
 */
bool rewrite_match_pattern(const char* pattern, const char* str) {
    const char* p = pattern;
    const char* s = str;
    const char* star = NULL;
    const char* str_match = NULL;
    
    while (*s) {
        if (*p == '*') {
            star = p++;
            str_match = s;
        } else if (*p == *s || *p == '?') {
            p++;
            s++;
        } else if (star) {
            p = star + 1;
            s = ++str_match;
        } else {
            return false;
        }
    }
    
    while (*p == '*') p++;
    return *p == '\0';
}

/*
 * Replace pattern in string (simple wildcard replacement).
 */
static void replace_pattern(const char* pattern, const char* replacement,
                           const char* input, char* output, size_t output_size) {
    /* Simple implementation: replace * with matched portion */
    const char* star = strchr(pattern, '*');
    if (!star) {
        /* No wildcard - direct replacement */
        strncpy(output, replacement, output_size - 1);
        output[output_size - 1] = '\0';
        return;
    }
    
    /* Find match position */
    size_t before_star = star - pattern;
    if (strncmp(input, pattern, before_star) != 0) {
        strncpy(output, input, output_size - 1);
        output[output_size - 1] = '\0';
        return;
    }
    
    /* Find end of match */
    const char* after_star = star + 1;
    const char* input_match_start = input + before_star;
    const char* input_match_end = input_match_start;
    
    if (*after_star) {
        /* Find where pattern continues */
        const char* end_pattern = strstr(input_match_start, after_star);
        if (end_pattern) {
            input_match_end = end_pattern;
        } else {
            input_match_end = input + strlen(input);
        }
    } else {
        input_match_end = input + strlen(input);
    }
    
    /* Build replacement */
    size_t match_len = input_match_end - input_match_start;
    const char* rep_star = strchr(replacement, '*');
    
    if (!rep_star) {
        /* No * in replacement - just use replacement */
        strncpy(output, replacement, output_size - 1);
        output[output_size - 1] = '\0';
    } else {
        /* Replace * with matched portion */
        size_t before_rep = rep_star - replacement;
        size_t after_rep_len = strlen(rep_star + 1);
        
        if (before_rep + match_len + after_rep_len >= output_size) {
            match_len = output_size - before_rep - after_rep_len - 1;
        }
        
        strncpy(output, replacement, before_rep);
        strncpy(output + before_rep, input_match_start, match_len);
        strncpy(output + before_rep + match_len, rep_star + 1, after_rep_len);
        output[before_rep + match_len + after_rep_len] = '\0';
    }
}

/*
 * Create rewrite engine.
 */
BoltRewriteEngine* rewrite_engine_create(void) {
    BoltRewriteEngine* engine = (BoltRewriteEngine*)calloc(1, sizeof(BoltRewriteEngine));
    return engine;
}

/*
 * Destroy rewrite engine.
 */
void rewrite_engine_destroy(BoltRewriteEngine* engine) {
    if (!engine) return;
    
    BoltRewriteRule* rule = engine->rules;
    while (rule) {
        BoltRewriteRule* next = rule->next;
        free(rule);
        rule = next;
    }
    
    free(engine);
}

/*
 * Add a rewrite rule.
 */
bool rewrite_add_rule(BoltRewriteEngine* engine, const char* pattern,
                      const char* replacement, RewriteType type) {
    if (!engine || !pattern || !replacement) return false;
    
    BoltRewriteRule* rule = (BoltRewriteRule*)calloc(1, sizeof(BoltRewriteRule));
    if (!rule) return false;
    
    strncpy(rule->pattern, pattern, sizeof(rule->pattern) - 1);
    strncpy(rule->replacement, replacement, sizeof(rule->replacement) - 1);
    rule->type = type;
    
    rule->next = engine->rules;
    engine->rules = rule;
    
    return true;
}

/*
 * Apply rewrite rules to a URI.
 */
bool rewrite_apply(BoltRewriteEngine* engine, const char* uri, char* out_uri, size_t out_size) {
    if (!engine || !uri || !out_uri || out_size == 0) return false;
    
    BoltRewriteRule* rule = engine->rules;
    while (rule) {
        if (rewrite_match_pattern(rule->pattern, uri)) {
            replace_pattern(rule->pattern, rule->replacement, uri, out_uri, out_size);
            return true;
        }
        rule = rule->next;
    }
    
    /* No match - copy original */
    strncpy(out_uri, uri, out_size - 1);
    out_uri[out_size - 1] = '\0';
    return false;
}

