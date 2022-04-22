#include "stdlib.h"
#include "syscall.h"
#include <common/extra.h>
#include <kernel/api/dirent.h>
#include <kernel/api/stat.h>
#include <string.h>

static int get_format(const struct dirent* dent, const char** out_format,
                      size_t* out_len) {
    struct stat stat_buf;
    if (stat(dent->name, &stat_buf) < 0)
        return -1;
    switch (stat_buf.st_mode & S_IFMT) {
    case S_IFDIR:
        *out_format = "\x1b[01;34m%s\x1b[m/";
        *out_len = strlen(dent->name) + 1;
        break;
    case S_IFCHR:
    case S_IFBLK:
        *out_format = "\x1b[01;33m%s\x1b[m";
        *out_len = strlen(dent->name);
        break;
    case S_IFIFO:
        *out_format = "\x1b[33m%s\x1b[m|";
        *out_len = strlen(dent->name) + 1;
        break;
    case S_IFLNK:
        *out_format = "\x1b[01;36m%s\x1b[m@";
        *out_len = strlen(dent->name) + 1;
        break;
    case S_IFSOCK:
        *out_format = "\x1b[01;35m%s\x1b[m=";
        *out_len = strlen(dent->name) + 1;
        break;
    case S_IFREG:
    default:
        if (stat_buf.st_mode & S_IXUSR) {
            *out_format = "\x1b[01;32m%s\x1b[m*";
            *out_len = strlen(dent->name) + 1;
        } else {
            *out_format = "%s";
            *out_len = strlen(dent->name);
        }
        break;
    }
    return 0;
}

#define TAB_STOP 8

int main(int argc, char* argv[]) {
    const char* path;
    if (argc < 2) {
        static char path_buf[1024];
        getcwd(path_buf, 1024);
        path = path_buf;
    } else {
        path = argv[1];
    }

    DIR* dirp = opendir(path);
    if (!dirp) {
        perror("opendir");
        return EXIT_FAILURE;
    }
    chdir(path);

    size_t width = 0;
    for (;;) {
        struct dirent* dent = readdir(dirp);
        if (!dent)
            break;

        const char* format;
        size_t len;
        if (get_format(dent, &format, &len) < 0) {
            perror("get_format");
            return EXIT_FAILURE;
        }
        size_t next_pos = round_up(width + len + 1, TAB_STOP);
        if (next_pos >= 80) {
            width = round_up(len + 1, TAB_STOP);
            putchar('\n');
        } else {
            width = next_pos;
        }
        printf(format, dent->name);
        putchar('\t');
    }
    if (width > 0)
        putchar('\n');

    closedir(dirp);
    return EXIT_SUCCESS;
}
