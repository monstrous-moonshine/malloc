#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

struct header_t {
    size_t size;
    struct header_t *next;
};
static_assert(sizeof(struct header_t) == 16, "");

static inline int is_used(struct header_t *h) {
    return (uintptr_t)h & 1;
}

static inline void set_used(struct header_t *h) {
    uintptr_t t = (uintptr_t)h->next | 1;
    h->next = (void *)t;
}

static inline void set_next(struct header_t *h, void *next) {
    int u = is_used(h);
    h->next = next;
    if (u) set_used(h);
}

static pthread_mutex_t g_malloc_mtx = PTHREAD_MUTEX_INITIALIZER;
static struct header_t *head = NULL, *tail = NULL;

static void *get_free_block(size_t size) {
    for (struct header_t *c = head; c; c = c->next) {
        if (!is_used(c) && c->size >= size)
            return c;
    }
    return NULL;
}

void *malloc(size_t size) {
    if (!size) return NULL;
    pthread_mutex_lock(&g_malloc_mtx);
    struct header_t *block = get_free_block(size);
    if (block) {
        set_used(block);
        pthread_mutex_unlock(&g_malloc_mtx);
        return block + 1;
    }
    block = sbrk(size + sizeof *block);
    if (block == (void *)-1) {
        pthread_mutex_unlock(&g_malloc_mtx);
        errno = ENOMEM;
        return NULL;
    }
    block->size = size;
    block->next = NULL;
    set_used(block);
    if (!head) head = block;
    if (tail) {
        set_next(tail, block);
    }
    tail = block;
    pthread_mutex_unlock(&g_malloc_mtx);
    return block + 1;
}

void free(void *block) {
    if (!block) return;
}

void *calloc(size_t nmemb, size_t size) {
    if (!nmemb || !size) return NULL;
    size_t alloc_size;
    if (__builtin_mul_overflow(nmemb, size, &alloc_size)) {
        /* XXX: how to set errno here? */
        return NULL;
    }
    void *buf = malloc(alloc_size);
    if (!buf) {
        errno = ENOMEM;
        return NULL;
    }
    memset(buf, 0, alloc_size);
    return buf;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (!size) free(ptr);
    struct header_t *header = (struct header_t *)ptr - 1;
    if (header->size > size) return ptr;
    void *buf = malloc(size);
    if (!buf) {
        errno = ENOMEM;
        return NULL;
    }
    memcpy(buf, ptr, header->size);
    free(ptr);
    return buf;
}
