#include <ctype.h>
#include <string.h>

#include "util.h"


/* left skip */
char *
cskip_l(const char *dest)
{
	while (*dest && isspace((unsigned char)(*dest)))
		dest++;

	return (char *)dest;
}

/* right skip */
char *
cskip_r(char *dest,
	size_t len)
{
	char *end;

	if (len == 0)
		len = strlen(dest);

	end = dest + (len -1u);
	while (end > dest && isspace((unsigned char)(*end)))
		end--;

	*(end +1u) = '\0';

	return dest;
}

/* right + left skip */
char *
cskip_rl(char *dest,
	 size_t len)
{
	return cskip_l(cskip_r(dest, len));
}

/* skip all */
char *
cskip_a(char *dest)
{
	char *p = dest;
	char *s = dest;

	while (*p != '\0') {
		if (isspace((unsigned char)*p)) {
			p++;

			continue;
		}

		*(s++) = *(p++);
	}
	*s = '\0';

	return dest;
}

/* skipping html tags ( <b>...</b> ) */
char *
cskip_html_tags(char *dest,
	       size_t len)
{
	const struct {
		const char *const tags[2];
		size_t            len[2];

	} tag_list[] = {
		{ .tags = { "<b>", "</b>" }, .len = { 3, 4 } },
		{ .tags = { "<i>", "</i>" }, .len = { 3, 4 } },

		/* We don't really need these (probably):
		{ .tags = { "<u>", "</u>" }, .len = { 3, 4 } },
		{ .tags = { "<br>", ""    }, .len = { 4, 0 } }, */
	};

	const char *end;
	char       *tag_open  = NULL;
	char       *tag_close = NULL;
	size_t      i = 0, hi;


	if (len == 0)
		len = strlen(dest);

	end = dest + (len -1u);

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
	} while (i < len);

	return dest;
}

char *
url_encode(char *dest,
	   const char *src,
	   size_t len)
{
	const char *const hex_list = "0123456789abcdef";
	const unsigned char *p     = (const unsigned char *)src;
	size_t i   = 0;
	size_t pos = 0;


	if (len == 0)
		len = strlen(src);

	while (p[i] != '\0' && i < len) {
		if (!isalnum((unsigned char)p[i])) {
			dest[pos++] = '%';
			dest[pos++] = hex_list[(p[i] >> 4u) & 15u];
			dest[pos++] = hex_list[p[i] & 15u];

			i++;
			continue;
		}

		dest[pos++] = p[i++];
	}

	dest[pos] = '\0';

	return dest;
}
