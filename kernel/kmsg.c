#include "kmsg.h"
#include "drivers/serial.h"
#include "lock.h"
#include <common/stdio.h>
#include <common/string.h>
#include <stdarg.h>

int kprint(const char* str) {
    size_t len = strlen(str);
    kmsg_write(str, len);
    return len;
}

int kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = kvprintf(format, args);
    va_end(args);
    return ret;
}

int kvprintf(const char* format, va_list args) {
    char buf[1024];
    int ret = vsnprintf(buf, sizeof(buf), format, args);
    kprint(buf);
    return ret;
}

static char ring_buf[KMSG_BUF_SIZE];
static size_t read_index = 0;
static size_t write_index = 0;
static struct spinlock lock;

size_t kmsg_read(char* buf, size_t count) {
    size_t dest_index = 0;

    spinlock_lock(&lock);
    for (size_t src_index = read_index;
         dest_index < count && src_index != write_index;
         src_index = (src_index + 1) % KMSG_BUF_SIZE)
        buf[dest_index++] = ring_buf[src_index];
    spinlock_unlock(&lock);

    return dest_index;
}

void kmsg_write(const char* buf, size_t count) {
    spinlock_lock(&lock);
    for (size_t i = 0; i < count; ++i) {
        ring_buf[write_index] = buf[i];
        write_index = (write_index + 1) % KMSG_BUF_SIZE;
        if (write_index == read_index)
            read_index = (read_index + 1) % KMSG_BUF_SIZE;
    }
    serial_write(0, buf, count);
    spinlock_unlock(&lock);
}
