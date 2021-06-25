#ifndef UTIL_H
#define UTIL_H

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRING_NEW() (calloc(1, 1))
#define LENGTH(X) ((sizeof(X) / (sizeof(X[0]))))

void die(const char *fmt, ...);
char *ltrim(const char *str);
char *rtrim(char *str);
void string_append(char **dest, const char *fmt, ...);
void trim_tag(char **str, char tag);

#endif
