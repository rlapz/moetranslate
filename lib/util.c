#include <ctype.h>
#include <string.h>

#include "util.h"

/* left trim */
char *
lskip(const char *str)
{
	while (*str && isspace((unsigned char)(*str)))
		str++;

	return (char*)str;
}

/* right trim */
char *
rskip(char *str)
{
	char *end = str + strlen(str) -1;
	while (end > str && isspace((unsigned char)(*end)))
		end--;

	*(end+1) = '\0';

	return str;
}

/* trim html tag ( <b>...</b> ) */
char *
skip_html_tags(char *dest)
{
#define B_SIZE 256
	const char *hlist = "abiu";
	char *p = dest;
	char tmp[B_SIZE];
	size_t i = 0, j = 0;

	while (p[i] != '\0' && j < B_SIZE) {
		const char *pl = hlist;

		while (*pl) {
			if (p[i] == '<' && p[i+1] != '/' && p[i+1] == *pl &&
					p[i+2] == '>') {
				i += 3;
			}

			if (p[i] == '<' && p[i+1] == '/' && p[i+2] == *pl &&
					p[i+3] == '>') {
				i += 4;
			}
			pl++;
		}

		tmp[j] = p[i];
		j++;

		if (p[i] == '\0')
			break;
		i++;
	}

	memcpy(p, tmp, j);
	p[j] = '\0';

	return dest;
}

char *
url_encode(char *dest, const char *src, size_t len)
{
	const char *const hex  = "0123456789abcdef";
	const unsigned char *p = (unsigned char *)src;
	size_t i   = 0;
	size_t pos = 0;

	while (p[i] != '\0' && i < len) {
		if (isalnum(p[i])) {
			dest[pos++] = p[i];
		} else {
			dest[pos++] = '%';
			dest[pos++] = hex[p[i] >> 4u];
			dest[pos++] = hex[p[i] & 15u];
		}

		i++;
	}

	dest[pos] = '\0';

	return dest;
}
