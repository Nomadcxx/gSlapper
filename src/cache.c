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

// Find entry by path (internal, caller must hold mutex)
static cache_entry_t *find_entry(const char *path) {
    if (!g_cache || !path) return NULL;

    cache_entry_t *entry = g_cache->entries;
    while (entry) {
        if (strcmp(entry->path, path) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

cache_entry_t *cache_get(const char *path) {
    if (!g_cache || !g_cache->enabled || !path) return NULL;

    pthread_mutex_lock(&g_cache->mutex);

    cache_entry_t *entry = find_entry(path);
    if (entry) {
        // Update LRU timestamp
        entry->last_used = get_timestamp_ns();
    }

    pthread_mutex_unlock(&g_cache->mutex);
    return entry;
}

bool cache_contains(const char *path) {
    if (!g_cache || !g_cache->enabled || !path) return false;

    pthread_mutex_lock(&g_cache->mutex);
    bool found = (find_entry(path) != NULL);
    pthread_mutex_unlock(&g_cache->mutex);

    return found;
}

// Evict least recently used entry (internal, caller must hold mutex)
static void evict_lru(void) {
    if (!g_cache || !g_cache->entries) return;

    cache_entry_t *lru = NULL;
    cache_entry_t *lru_prev = NULL;
    cache_entry_t *prev = NULL;
    cache_entry_t *entry = g_cache->entries;
    uint64_t oldest = UINT64_MAX;

    // Find LRU entry (prefer non-displayed entries)
    while (entry) {
        // Skip currently displayed entries unless they're the only option
        if (!entry->currently_displayed && entry->last_used < oldest) {
            oldest = entry->last_used;
            lru = entry;
            lru_prev = prev;
        }
        prev = entry;
        entry = entry->next;
    }

    // If all entries are displayed, evict oldest anyway
    if (!lru) {
        prev = NULL;
        entry = g_cache->entries;
        oldest = UINT64_MAX;
        while (entry) {
            if (entry->last_used < oldest) {
                oldest = entry->last_used;
                lru = entry;
                lru_prev = prev;
            }
            prev = entry;
            entry = entry->next;
        }
    }

    if (!lru) return;

    // Remove from list
    if (lru_prev) {
        lru_prev->next = lru->next;
    } else {
        g_cache->entries = lru->next;
    }

    g_cache->total_size -= lru->size;
    g_cache->entry_count--;

    cflp_info("Cache evicted (LRU): %s (%.2f MB)",
              lru->path, (double)lru->size / (1024 * 1024));

    free(lru->path);
    free(lru->data);
    free(lru);
}

cache_entry_t *cache_add(const char *path, unsigned char *data,
                         int width, int height) {
    if (!g_cache || !g_cache->enabled || !path || !data) {
        free(data);  // Take ownership, must free if not caching
        return NULL;
    }

    size_t size = (size_t)width * height * 4;  // RGBA

    pthread_mutex_lock(&g_cache->mutex);

    // Check if already cached
    cache_entry_t *existing = find_entry(path);
    if (existing) {
        pthread_mutex_unlock(&g_cache->mutex);
        free(data);  // Don't need duplicate
        return existing;
    }

    // Evict until we have space
    while (g_cache->total_size + size > g_cache->max_size &&
           g_cache->entries != NULL) {
        evict_lru();
    }

    // Create new entry
    cache_entry_t *entry = calloc(1, sizeof(cache_entry_t));
    if (!entry) {
        pthread_mutex_unlock(&g_cache->mutex);
        free(data);
        cflp_error("Failed to allocate cache entry");
        return NULL;
    }

    entry->path = strdup(path);
    entry->data = data;
    entry->size = size;
    entry->width = width;
    entry->height = height;
    entry->last_used = get_timestamp_ns();
    entry->currently_displayed = false;

    // Add to front of list
    entry->next = g_cache->entries;
    g_cache->entries = entry;

    g_cache->total_size += size;
    g_cache->entry_count++;

    cflp_info("Cache added: %s (%dx%d, %.2f MB, %d entries, %.1f/%.1f MB used)",
              path, width, height, (double)size / (1024 * 1024),
              g_cache->entry_count,
              (double)g_cache->total_size / (1024 * 1024),
              (double)g_cache->max_size / (1024 * 1024));

    pthread_mutex_unlock(&g_cache->mutex);
    return entry;
}
