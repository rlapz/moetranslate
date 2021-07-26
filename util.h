#ifndef UTIL_H
#define UTIL_H

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LENGTH(X) (sizeof(X) / sizeof(X[0]))

typedef struct {
	char   *value;
	size_t length;
} String;


void die(const char *fmt, ...);

char	*ltrim(const char *str);
char 	*rtrim(char *str);
void 	 trim_tag(char *dest, char tag);
char	*url_encode(char *dest, const char *src, size_t len);

#endif
