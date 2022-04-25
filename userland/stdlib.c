#include "stdlib.h"
#include "syscall.h"
#include <common/calendar.h>
#include <common/extra.h>
#include <common/string.h>
#include <kernel/api/dirent.h>
#include <kernel/api/errno.h>
#include <kernel/api/fcntl.h>
#include <kernel/api/mman.h>
#include <kernel/api/signum.h>
#include <kernel/api/syscall.h>
#include <kernel/api/unistd.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* const argv[], char* const envp[]);

char** environ;

void _start(int argc, char* const argv[], char* const envp[]) {
    environ = (char**)envp;
    exit(main(argc, argv, envp));
}

noreturn void abort(void) { exit(128 + SIGABRT); }

noreturn void panic(const char* message, const char* file, size_t line) {
    dprintf(STDERR_FILENO, "%s at %s:%u\n", message, file, line);
    abort();
}

#define MALLOC_HEAP_SIZE 0x100000

static struct {
    bool initialized;
    uintptr_t heap_start;
    uintptr_t ptr;
    size_t num_allocs;
} malloc_ctx;

static void malloc_init_if_needed(void) {
    if (malloc_ctx.initialized)
        return;

    void* heap = mmap(NULL, MALLOC_HEAP_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    ASSERT(heap != MAP_FAILED);
    malloc_ctx.heap_start = malloc_ctx.ptr = (uintptr_t)heap;
    malloc_ctx.num_allocs = 0;
    malloc_ctx.initialized = true;
}

void* aligned_alloc(size_t alignment, size_t size) {
    if (size == 0)
        return NULL;

    malloc_init_if_needed();

    uintptr_t aligned_ptr = round_up(malloc_ctx.ptr, alignment);
    uintptr_t next_ptr = aligned_ptr + size;
    if (next_ptr > malloc_ctx.heap_start + MALLOC_HEAP_SIZE) {
        errno = ENOMEM;
        return NULL;
    }

    memset((void*)aligned_ptr, 0, size);

    malloc_ctx.ptr = next_ptr;
    ++malloc_ctx.num_allocs;

    return (void*)aligned_ptr;
}

void* malloc(size_t size) { return aligned_alloc(alignof(max_align_t), size); }

void free(void* ptr) {
    if (!ptr)
        return;

    ASSERT(malloc_ctx.initialized);
    ASSERT(malloc_ctx.num_allocs > 0);
    if (--malloc_ctx.num_allocs == 0)
        malloc_ctx.ptr = malloc_ctx.heap_start;
}

char* strdup(const char* src) {
    size_t len = strlen(src);
    char* buf = malloc((len + 1) * sizeof(char));
    if (!buf)
        return NULL;

    memcpy(buf, src, len);
    buf[len] = '\0';
    return buf;
}

int putchar(int ch) {
    char c = ch;
    if (write(STDOUT_FILENO, &c, 1) < 0)
        return -1;
    return ch;
}

int puts(const char* str) {
    int rc = write(STDOUT_FILENO, str, strlen(str));
    if (rc < 0)
        return -1;
    if (write(STDOUT_FILENO, "\n", 1) < 0)
        return -1;
    return rc + 1;
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vdprintf(STDOUT_FILENO, format, args);
    va_end(args);
    return ret;
}

int dprintf(int fd, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vdprintf(fd, format, args);
    va_end(args);
    return ret;
}

int vdprintf(int fd, const char* format, va_list ap) {
    char buf[1024];
    int len = vsnprintf(buf, 1024, format, ap);
    return write(fd, buf, len);
}

int errno;

#define ERRNO_MSG(I, MSG) MSG,
static const char* errno_msgs[EMAXERRNO] = {ENUMERATE_ERRNO(ERRNO_MSG)};
#undef ERRNO_MSG

char* strerror(int errnum) {
    if (0 <= errnum && errnum < EMAXERRNO)
        return (char*)errno_msgs[errnum];
    return "Unknown error";
}

void perror(const char* s) {
    dprintf(STDERR_FILENO, "%s: %s\n", s, strerror(errno));
}

char* getenv(const char* name) {
    for (char** env = environ; *env; ++env) {
        char* s = strchr(*env, '=');
        if (!s)
            continue;
        size_t len = s - *env;
        if (len > 0 && !strncmp(*env, name, len))
            return s + 1;
    }
    return NULL;
}

int execvpe(const char* file, char* const argv[], char* const envp[]) {
    if (strchr(file, '/'))
        return execve(file, argv, envp);

    const char* path = getenv("PATH");
    if (!path)
        path = "/bin";
    char* dup_path = strdup(path);
    if (!dup_path)
        return -1;

    int saved_errno = errno;

    static const char* sep = ":";
    char* saved_ptr;
    for (const char* part = strtok_r(dup_path, sep, &saved_ptr); part;
         part = strtok_r(NULL, sep, &saved_ptr)) {
        static char buf[1024];
        ASSERT(sprintf(buf, "%s/%s", part, file) > 0);
        int rc = execve(buf, argv, envp);
        ASSERT(rc < 0);
        if (errno != ENOENT)
            return -1;
        errno = saved_errno;
    }

    errno = ENOENT;
    return -1;
}

unsigned int sleep(unsigned int seconds) {
    struct timespec req = {.tv_sec = seconds, .tv_nsec = 0};
    struct timespec rem;
    if (nanosleep(&req, &rem) < 0)
        return rem.tv_sec;
    return 0;
}

#define DIR_BUF_CAPACITY 1024

typedef struct DIR {
    int fd;
    unsigned char buf[DIR_BUF_CAPACITY];
    size_t buf_size;
    size_t buf_cursor;
} DIR;

DIR* opendir(const char* name) {
    DIR* dirp = malloc(sizeof(DIR));
    if (!dirp)
        return NULL;
    dirp->fd = open(name, O_RDONLY);
    if (dirp->fd < 0)
        return NULL;
    return dirp;
}

int closedir(DIR* dirp) {
    int rc = close(dirp->fd);
    free(dirp);
    return rc;
}

struct dirent* readdir(DIR* dirp) {
    if (dirp->buf_cursor >= dirp->buf_size) {
        ssize_t nwritten = getdents(dirp->fd, dirp->buf, DIR_BUF_CAPACITY);
        if (nwritten <= 0)
            return NULL;
        dirp->buf_size = nwritten;
        dirp->buf_cursor = 0;
    }
    struct dirent* dent = (struct dirent*)(dirp->buf + dirp->buf_cursor);
    dirp->buf_cursor += dent->record_len;
    return dent;
}

clock_t clock(void) {
    struct tms tms;
    times(&tms);
    return tms.tms_utime + tms.tms_stime;
}

time_t time(time_t* tloc) {
    struct timespec tp;
    if (clock_gettime(CLOCK_REALTIME, &tp) < 0)
        return -1;
    if (tloc)
        *tloc = tp.tv_sec;
    return tp.tv_sec;
}

static int32_t divmodi64(int64_t a, int32_t b, int32_t* rem) {
    int32_t q;
    int32_t r;
    __asm__("idivl %[b]"
            : "=a"(q), "=d"(r)
            : "d"((int32_t)(a >> 32)),
              "a"((int32_t)(a & 0xffffffff)), [b] "rm"(b));
    *rem = r;
    return q;
}

struct tm* gmtime_r(const time_t* t, struct tm* tm) {
    static int const seconds_per_day = 60 * 60 * 24;

    time_t time = *t;

    unsigned year = 1970;
    for (;; ++year) {
        time_t seconds_in_this_year =
            (time_t)days_in_year(year) * seconds_per_day;
        if (time < seconds_in_this_year)
            break;
        time -= seconds_in_this_year;
    }
    tm->tm_year = year - 1900;

    int seconds;
    time_t days = divmodi64(time, seconds_per_day, &seconds);
    tm->tm_yday = days;
    tm->tm_sec = seconds % 60;

    int minutes = seconds / 60;
    tm->tm_hour = minutes / 60;
    tm->tm_min = minutes % 60;

    unsigned month;
    for (month = 1; month < 12; ++month) {
        time_t days_in_this_month = (time_t)days_in_month(year, month);
        if (days < days_in_this_month)
            break;
        days -= days_in_this_month;
    }

    tm->tm_mon = month - 1;
    tm->tm_mday = days + 1;
    tm->tm_wday = day_of_week(year, month, tm->tm_mday);

    return tm;
}

char* asctime_r(const struct tm* time_ptr, char* buf) {
    static const char* day_names[] = {"Sun", "Mon", "Tue", "Wed",
                                      "Thu", "Fri", "Sat"};
    static const char* month_names[] = {"Jan", "Feb", "Mar", "Apr",
                                        "May", "Jun", "Jul", "Aug",
                                        "Sep", "Oct", "Nov", "Dec"};
    int len = sprintf(
        buf, "%s %s %2d %02d:%02d:%02d %d", day_names[time_ptr->tm_wday],
        month_names[time_ptr->tm_mon], time_ptr->tm_mday, time_ptr->tm_hour,
        time_ptr->tm_min, time_ptr->tm_sec, time_ptr->tm_year + 1900);
    return len > 0 ? buf : NULL;
}
