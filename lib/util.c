#include <ctype.h>
#include <string.h>

#include "util.h"


/* left skip */
char *
lskip(const char *str)
{
	while (*str && isspace((unsigned char)(*str)))
		str++;

	return (char*)str;
}

/* right skip */
char *
rskip(char *str)
{
	char *end = str + strlen(str) -1;
	while (end > str && isspace((unsigned char)(*end)))
		end--;

	*(end+1) = '\0';

	return str;
}

/* skipping html tag ( <b>...</b> ) */
char *
skip_html_tags(char *dest, size_t size)
{
	const struct {
		const char *const tags[2];
		size_t            len[2];

	} tag_list[] = {
		{ .tags = { "<a>", "</a>" }, .len = { 3, 4 } },
		{ .tags = { "<b>", "</b>" }, .len = { 3, 4 } },
		{ .tags = { "<i>", "</i>" }, .len = { 3, 4 } },
		{ .tags = { "<u>", "</u>" }, .len = { 3, 4 } },
		{ .tags = { "<br>", ""    }, .len = { 4, 0 } },
	};

	const char *end       = dest + (size -1);
	char       *tag_open  = NULL;
	char       *tag_close = NULL;
	size_t      i = 0, hi;

	do {
		hi = 0;
		for (; hi < LENGTH(tag_list); hi++) {
			if ((tag_open = strstr(dest + i, tag_list[hi].tags[0])) != NULL) {
				if ((tag_close = strstr(tag_open, tag_list[hi].tags[1])) != NULL) {
					memmove(tag_open, tag_open + tag_list[hi].len[0], end - tag_open);

					tag_close -= tag_list[hi].len[0];

					memmove(tag_close, tag_close + tag_list[hi].len[1], end - tag_close);
				}
			}
		}

		i += (end - tag_close);
	} while (i < size);


	return dest;
}

char *
url_encode(char *dest, const char *src, size_t len)
{
	const char *const hex_list = "0123456789abcdef";
	const unsigned char *p     = (unsigned char *)src;
	size_t i   = 0;
	size_t pos = 0;

	while (p[i] != '\0' && i < len) {
		if (isalnum(p[i])) {
			dest[pos++] = p[i];
		} else {
			dest[pos++] = '%';
			dest[pos++] = hex_list[p[i] >> 4u];
			dest[pos++] = hex_list[p[i] & 15u];
		}

		i++;
	}

	dest[pos] = '\0';

	return dest;
}
