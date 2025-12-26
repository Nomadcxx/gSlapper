#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cache.h"
#include "cflogprinter.h"

// Cache manager structure
struct image_cache {
    cache_entry_t *entries;        // Linked list head
    size_t total_size;             // Current cache size in bytes
    size_t max_size;               // Limit in bytes
    pthread_mutex_t mutex;         // Thread safety
    int entry_count;               // Number of cached images
    bool enabled;                  // False if max_size == 0
};

// Global cache instance
static image_cache_t *g_cache = NULL;

// Get monotonic timestamp in nanoseconds
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void cache_init(size_t max_size_mb) {
    if (g_cache != NULL) {
        cflp_warning("Cache already initialized");
        return;
    }

    g_cache = calloc(1, sizeof(image_cache_t));
    if (!g_cache) {
        cflp_error("Failed to allocate cache manager");
        return;
    }

    g_cache->max_size = max_size_mb * 1024 * 1024;  // Convert to bytes
    g_cache->enabled = (max_size_mb > 0);
    pthread_mutex_init(&g_cache->mutex, NULL);

    if (g_cache->enabled) {
        cflp_info("Image cache initialized: %zu MB limit", max_size_mb);
    } else {
        cflp_info("Image cache disabled");
    }
}

void cache_shutdown(void) {
    if (!g_cache) return;

    pthread_mutex_lock(&g_cache->mutex);

    // Free all entries
    cache_entry_t *entry = g_cache->entries;
    while (entry) {
        cache_entry_t *next = entry->next;
        free(entry->path);
        free(entry->data);
        free(entry);
        entry = next;
    }

    pthread_mutex_unlock(&g_cache->mutex);
    pthread_mutex_destroy(&g_cache->mutex);

    free(g_cache);
    g_cache = NULL;

    cflp_info("Image cache shutdown");
}

bool cache_enabled(void) {
    return g_cache && g_cache->enabled;
}

void cache_stats(size_t *used_bytes, size_t *max_bytes, int *entry_count) {
    if (!g_cache) {
        if (used_bytes) *used_bytes = 0;
        if (max_bytes) *max_bytes = 0;
        if (entry_count) *entry_count = 0;
        return;
    }

    pthread_mutex_lock(&g_cache->mutex);
    if (used_bytes) *used_bytes = g_cache->total_size;
    if (max_bytes) *max_bytes = g_cache->max_size;
    if (entry_count) *entry_count = g_cache->entry_count;
    pthread_mutex_unlock(&g_cache->mutex);
}
