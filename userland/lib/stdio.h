#pragma once

#include <common/stdio.h>
#include <kernel/api/stdio.h>

int putchar(int ch);
int puts(const char* str);
int printf(const char* format, ...);
int dprintf(int fd, const char* format, ...);
int vdprintf(int fd, const char* format, va_list ap);

void perror(const char*);

int getchar(void);

int dbgputs(const char* str);
