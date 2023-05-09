#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hashtable.h"
#include "cache.h"

/**
 * Allocate a cache entry
 */
cache_entry *alloc_entry(char *path, char *content_type, void *content, int content_length)
{
    cache_entry *entry = malloc(sizeof(cache_entry));
    if (entry == NULL) {
        perror("cache entry alloc failed\n\r");
        return NULL;
    }
    memset(entry, 0, sizeof(cache_entry));
    entry->content = malloc(sizeof(content_length));
    if (entry->content == NULL) {
        perror("cache entry content alloc failed\n\r");
        free(entry);
        return NULL;
    }
    entry->path = malloc(strlen(path) + 1);
    if (entry->path == NULL) {
        perror("cache entry content alloc failed\n\r");
        free(entry->content);
        free(entry);
        return NULL;
    }
    entry->content_type = malloc(strlen(content_type) + 1);
    if (entry->content_type == NULL) {
        perror("cache entry content alloc failed\n\r");
        free(entry->content);
        free(entry->path);
        free(entry);
        return NULL;
    }
    strcpy(entry->path, path);
    strcpy(entry->content_type, content_type);
    memcpy(entry->content, content, content_length);
    entry->content_length = content_length;
    return entry;
}

/**
 * Deallocate a cache entry
 */
void free_entry(cache_entry *entry)
{
    free(entry->path);
    free(entry->content);
    free(entry->content_type);
    free(entry);
}

/**
 * Insert a cache entry at the head of the linked list
 */
void dllist_insert_head(cache *cache, cache_entry *ce)
{
    // Insert at the head of the list
    if (cache->head == NULL) {
        cache->head = cache->tail = ce;
        ce->prev = ce->next = NULL;
    } else {
        cache->head->prev = ce;
        ce->next = cache->head;
        ce->prev = NULL;
        cache->head = ce;
    }
}

/**
 * Move a cache entry to the head of the list
 */
void dllist_move_to_head(cache *cache, cache_entry *ce)
{
    if (ce != cache->head) {
        if (ce == cache->tail) {
            // We're the tail
            cache->tail = ce->prev;
            cache->tail->next = NULL;

        } else {
            // We're neither the head nor the tail
            ce->prev->next = ce->next;
            ce->next->prev = ce->prev;
        }

        ce->next = cache->head;
        cache->head->prev = ce;
        ce->prev = NULL;
        cache->head = ce;
    }
}


/**
 * Removes the tail from the list and returns it
 * 
 * NOTE: does not deallocate the tail
 */
cache_entry *dllist_remove_tail(cache *cache)
{
    cache_entry *oldtail = cache->tail;

    cache->tail = oldtail->prev;
    cache->tail->next = NULL;

    cache->cur_size--;

    return oldtail;
}

/**
 * Create a new cache
 * 
 * max_size: maximum number of entries in the cache
 * hashsize: hashtable size (0 for default)
 */
cache *cache_create(int max_size, int hashsize)
{
    cache *the_cache = malloc(sizeof(cache));
    if (the_cache == NULL) {
        perror("cache create failed");
        return NULL;
    }
    the_cache->max_size = max_size;
    the_cache->cur_size = 0;
    the_cache->head = NULL;
    the_cache->tail = NULL;
    struct hashtable *hash = hashtable_create(hashsize, NULL);
    if (hash == NULL) {
        perror("cache create hashtable failed");
        free(the_cache);
        return NULL;
    }
    return the_cache;
}

void cache_free(cache *cache)
{
    cache_entry *cur_entry = cache->head;

    hashtable_destroy(cache->index);

    while (cur_entry != NULL) {
        cache_entry *next_entry = cur_entry->next;

        free_entry(cur_entry);

        cur_entry = next_entry;
    }

    free(cache);
}

/**
 * Store an entry in the cache
 *
 * This will also remove the least-recently-used items as necessary.
 * 
 * NOTE: doesn't check for duplicate cache entries
 */
void cache_put(cache *cache, char *path, char *content_type, void *content, int content_length)
{
    cache_entry *entry = hashtable_get(cache->index, path);
    if (entry == NULL) {
        if (cache->cur_size == cache->max_size) {
            cache_entry *old_tail = dllist_remove_tail(cache);
            hashtable_delete(cache->index, old_tail->path);
            free_entry(old_tail);
        }
        cache_entry *target = alloc_entry(path, content_type, content, content_length);
        dllist_insert_head(cache, target);
        hashtable_put(cache->index, path, target);
    } else {
        dllist_move_to_head(cache, entry);
    }
    return;
}

/**
 * Retrieve an entry from the cache
 */
cache_entry *cache_get(cache *cache, char *path)
{
    cache_entry *entry = hashtable_get(cache->index, path);
    return entry;
}
