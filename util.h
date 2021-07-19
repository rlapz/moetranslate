#ifndef UTIL_H
#define UTIL_H

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LENGTH(X) (sizeof(X) / sizeof(X[0]))

typedef struct {
	char *value;
	size_t length;
} String;


void die(const char *fmt, ...);

char *ltrim(const char *str);
char *rtrim(char *str);

size_t append_string(String *dest, const char *fmt, ...);
void free_string(String *dest);
String *new_string(void);

void trim_tag(String *str, char tag);

#endif
