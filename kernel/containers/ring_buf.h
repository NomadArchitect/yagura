#pragma once

#include <kernel/api/errno.h>
#include <kernel/api/sys/types.h>
#include <kernel/memory/memory.h>

struct ring_buf {
    size_t capacity;
    atomic_size_t write_index;
    atomic_size_t read_index;
    unsigned char* ring;
};

NODISCARD static inline int ring_buf_init(struct ring_buf* b, size_t capacity) {
    *b = (struct ring_buf){.capacity = capacity};
    b->ring = kmalloc(capacity);
    if (!b->ring)
        return -ENOMEM;
    return 0;
}

static inline void ring_buf_destroy(struct ring_buf* b) { kfree(b->ring); }

static inline bool ring_buf_is_empty(const struct ring_buf* b) {
    return b->write_index == b->read_index;
}

static inline bool ring_buf_is_full(const struct ring_buf* b) {
    return (b->write_index + 1) % b->capacity == b->read_index;
}

NODISCARD static inline ssize_t ring_buf_read(struct ring_buf* b, void* bytes,
                                              size_t count) {
    size_t nread = 0;
    unsigned char* dest = bytes;
    const unsigned char* src = b->ring;
    while (nread < count) {
        dest[nread++] = src[b->read_index];
        b->read_index = (b->read_index + 1) % b->capacity;
        if (b->read_index == b->write_index)
            break;
    }
    return nread;
}

NODISCARD static inline ssize_t
ring_buf_write(struct ring_buf* b, const void* bytes, size_t count) {
    size_t nwritten = 0;
    unsigned char* dest = b->ring;
    const unsigned char* src = bytes;
    while (nwritten < count) {
        dest[b->write_index] = src[nwritten++];
        b->write_index = (b->write_index + 1) % b->capacity;
        if ((b->write_index + 1) % b->capacity == b->read_index)
            break;
    }
    return nwritten;
}

static inline ssize_t ring_buf_write_evicting_oldest(struct ring_buf* b,
                                                     const void* bytes,
                                                     size_t count) {
    size_t nwritten = 0;
    unsigned char* dest = b->ring;
    const unsigned char* src = bytes;
    while (nwritten < count) {
        dest[b->write_index] = src[nwritten++];
        b->write_index = (b->write_index + 1) % b->capacity;
        if (b->write_index == b->read_index)
            b->read_index = (b->read_index + 1) % b->capacity;
    }
    return nwritten;
}

static inline void ring_buf_clear(struct ring_buf* b) {
    b->write_index = b->read_index = 0;
}
