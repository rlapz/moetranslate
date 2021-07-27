#include "util.h"

void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
} 

/* left trim */
char *
ltrim(const char *str)
{
	while (*str && isspace((unsigned char)(*str)))
		str++;
	return (char*)str;
}

/* right trim */
char *
rtrim(char *str)
{
	char *end = str + strlen(str) -1;
	while (end > str && isspace((unsigned char)(*end))) {
		*end = '\0';
		end--;
	}
	return str;
}

/* trim html tag ( <b>...</b> ) */
void
trim_tag(char *dest, char tag)
{
#define B_SIZE 256
	char   *p = dest;
	char   tmp[B_SIZE];
	size_t i = 0, j = 0;

	/* UNSAFE */
	while (p[i] != '\0' && j < B_SIZE) {
		if (p[i] == '<' && p[i+1] != '/' && p[i+1] == tag &&
				p[i+2] == '>')
			i += 3;
		if (p[i] == '<' && p[i+1] == '/' && p[i+2] == tag &&
			       	p[i+3] == '>')
			i += 4;

		tmp[j] = p[i];
		j++;

		if (p[i] == '\0')
			break;
		i++;
	}
	strncpy(p, tmp, j);
	p[j] = '\0';
}

char *
url_encode(char *dest, const unsigned char *src, size_t len)
{
	const char *hex = "0123456789abcdef";
	size_t	    i  	= 0;
	size_t	    pos	= 0;

	printf("%d\n", src[0]);

	while (src[i] != '\0' && i < len) {
		if (isalnum(src[i])) {
			dest[pos++] = src[i];
		} else {
			dest[pos++] = '%';
			dest[pos++] = hex[src[i] >> 0x4];
			dest[pos++] = hex[src[i] & 0xf];
		}
		i++;
	}
	dest[pos] = '\0';

	return dest;
}
