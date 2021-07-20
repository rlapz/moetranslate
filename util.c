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
trim_tag(String *dest, char tag)
{
#define B_SIZE 1024
	char *p	= dest->value;
	char tmp[B_SIZE];
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
	/* don't forget to update string length */
	dest->length = j;
}

int
append_string(String *dest, const char *fmt, ...)
{
	int n;
	va_list v;
	size_t len  = 0;
	char *new_s = NULL;

	if (dest == NULL || dest->value == NULL)
		return 0;

	/* determine required size */
	va_start(v, fmt);
	n = vsnprintf(new_s, len, fmt, v);
	va_end(v);

	if (n < 0)
		return 0;

	len	= (size_t)n;
	new_s	= realloc(dest->value, len + dest->length +1);
	if (new_s == NULL)
		return 0;

	va_start(v, fmt);
	n = vsnprintf(new_s + dest->length, len +1, fmt, v);
	va_end(v);

	if (n < 0)
		return 0;

	dest->value   = new_s; 
	dest->length += (size_t)len;

	return n;
}

void
free_string(String *dest)
{
	if (dest == NULL)
		return;

	if (dest->value == NULL)
		return;

	free(dest->value);
	dest->value = NULL;
	dest->length = 0;

	free(dest);
	dest = NULL;
}

String *
new_string(void)
{
	char *value = calloc(1, 1);
	if (value == NULL)
		return NULL;

	String *str = malloc(sizeof(String));
	if (str == NULL) {
		free(value);
		value = NULL;
		return NULL;
	}

	str->value  = value;
	str->length = 0;

	return str;
}

