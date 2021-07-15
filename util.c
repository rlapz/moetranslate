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

void
string_append(char **dest, const char *fmt, ...)
{
	char *p		= (*dest);
	char *tmp_p	= NULL;
	char *new_p	= NULL;
	int n		= 0;
	size_t size	= 0;
	va_list vargs;

	if (p == NULL)
		die("string_append(): dest = NULL");

	/* determine required size */
	va_start(vargs, fmt);
	n = (size_t)vsnprintf(new_p, size, fmt, vargs);
	va_end(vargs);

	if (n < 0)
		die("string_append(): n size");

	size = (size_t)n +1; /* one extra byte for '\0' */
	if ((new_p = malloc(size)) == NULL)
		die("string_append(): new_p");

	va_start(vargs, fmt);
	n = vsnprintf(new_p, size, fmt, vargs);
	va_end(vargs);

	if (n < 0) {
		free(new_p);
		die("string_append(): n size");
	}

	tmp_p = realloc(p, strlen(p) + size);
	if (tmp_p == NULL)
		die("string_append(): tmp_p");

	p = tmp_p;
	strncat(p, new_p, size -1);
	(*dest) = p;

	free(new_p);
}

/* trim html tag ( <b>...</b> ) */
void
trim_tag(char **dest, char tag)
{
#define B_SIZE 1024
	char *p	= (*dest);
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
}

