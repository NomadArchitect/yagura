#include "stdlib.h"
#include "syscall.h"
#include <kernel/api/fcntl.h>

int main(int argc, char* const argv[]) {
    if (argc < 2) {
        dprintf(2, "Usage: touch FILE\n");
        return EXIT_FAILURE;
    }

    const char* filename = argv[1];
    int fd = open(filename, O_CREAT, 0);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
