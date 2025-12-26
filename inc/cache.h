#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

// Single cached image entry
typedef struct cache_entry {
    char *path;                    // Absolute file path (key)
    unsigned char *data;           // Decoded RGBA pixel data
    size_t size;                   // Size in bytes (width * height * 4)
    int width;                     // Image width
    int height;                    // Image height
    uint64_t last_used;            // Timestamp for LRU (monotonic ns)
    bool currently_displayed;      // True if actively shown on a monitor
    struct cache_entry *next;      // Linked list pointer
} cache_entry_t;

// Cache manager (opaque, defined in cache.c)
typedef struct image_cache image_cache_t;

// Default cache size in MB
#define DEFAULT_CACHE_SIZE_MB 256

// Initialize cache with max size in MB (0 to disable caching)
void cache_init(size_t max_size_mb);

// Shutdown and free all cached memory
void cache_shutdown(void);

// Check if caching is enabled
bool cache_enabled(void);

// Get cached entry by path, returns NULL if not found
// Updates last_used timestamp on hit
cache_entry_t *cache_get(const char *path);

// Add image to cache, returns entry pointer
// May evict LRU entries if cache is full
// Takes ownership of data pointer (will free on eviction)
cache_entry_t *cache_add(const char *path, unsigned char *data,
                         int width, int height);

// Remove specific entry from cache
void cache_remove(const char *path);

// Clear entire cache
void cache_clear(void);

// Clear entries not currently displayed
void cache_clear_unused(void);

// Mark entry as currently displayed or not
void cache_set_displayed(const char *path, bool displayed);

// Get cache statistics
void cache_stats(size_t *used_bytes, size_t *max_bytes, int *entry_count);

// Format cache list for IPC response (caller provides buffer)
void cache_list(char *buffer, size_t buflen);

// Check if path is in cache
bool cache_contains(const char *path);

#endif // CACHE_H
