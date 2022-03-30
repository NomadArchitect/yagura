#include "syscall.h"

void spawn_process(const char* filename) {
    if (fork() == 0) {
        char* argv[] = {NULL};
        char* envp[] = {NULL};
        execve(filename, argv, envp);
    }
}

void _start(void) {
    spawn_process("/socket-test");
    exit(0);
}
