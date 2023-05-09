#ifndef _WEBCACHE_H_
#define _WEBCACHE_H_

// Individual hash table entry
typedef struct cache_entry_t {
    char *path;   // Endpoint path--key to the cache
    char *content_type;
    int content_length;
    void *content;

    struct cache_entry_t *prev, *next; // Doubly-linked list
} cache_entry;

// A cache
typedef struct cache_t {
    struct hashtable *index;
    cache_entry *head, *tail; // Doubly-linked list
    int max_size; // Maxiumum number of entries
    int cur_size; // Current number of entries
} cache;

extern cache_entry *alloc_entry(char *path, char *content_type, void *content, int content_length);
extern void free_entry(cache_entry *entry);
extern cache *cache_create(int max_size, int hashsize);
extern void cache_free(cache *cache);
extern void cache_put(cache *cache, char *path, char *content_type, void *content, int content_length);
extern cache_entry *cache_get(cache *cache, char *path);

#endif