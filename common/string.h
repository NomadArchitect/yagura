#pragma once

#include <stddef.h>

size_t strnlen(const char* str, size_t n);
size_t strlcpy(char* dst, const char* src, size_t size);
void str_replace_char(char* str, char from, char to);
char* strtok_r(char* str, const char* sep, char** last);
