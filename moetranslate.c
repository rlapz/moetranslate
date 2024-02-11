/* MIT License
 *
 * Copyright (c) 2024 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netdb.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "json.h"
#include "config.h"



#define LEN(X) ((sizeof(X)) / (sizeof(*X)))


#if (CONFIG_COLOR_ENABLED != 0)
	#define COLOR_REGULAR_GREEN(X)  "\033[00;" CONFIG_COLOR_GREEN  "m" X "\033[00m"
	#define COLOR_REGULAR_YELLOW(X) "\033[00;" CONFIG_COLOR_YELLOW "m" X "\033[00m"
	
	#define COLOR_BOLD_BLUE(X)      "\033[01;" CONFIG_COLOR_BLUE   "m" X "\033[00m"
	#define COLOR_BOLD_GREEN(X)     "\033[01;" CONFIG_COLOR_GREEN  "m" X "\033[00m"
	#define COLOR_BOLD_WHITE(X)     "\033[01;" CONFIG_COLOR_WHITE  "m" X "\033[00m"
	#define COLOR_BOLD_YELLOW(X)    "\033[01;" CONFIG_COLOR_YELLOW "m" X "\033[00m"
#else
	#define COLOR_REGULAR_GREEN(X)  X
	#define COLOR_REGULAR_YELLOW(X) X
	
	#define COLOR_BOLD_BLUE(X)      X
	#define COLOR_BOLD_GREEN(X)     X
	#define COLOR_BOLD_WHITE(X)     X
	#define COLOR_BOLD_YELLOW(X)    X
#endif


/*
 * cstr
 */
static size_t      cstr_trim_right(const char cstr[], size_t len);
static const char *cstr_trim_left(const char cstr[], size_t *len);
static char       *cstr_trim_right_mut(char cstr[]);
static char       *cstr_trim_left_mut(char cstr[]);
static char       *cstr_skip_html_tags(char raw[], size_t len);


/*
 * Buffer
 */
typedef struct {
	char   *ptr;
	size_t  size;
} Buffer;

static int   buffer_init(Buffer *b, size_t size);
static void  buffer_deinit(Buffer *b);

/* ret: -1 -> failed to realloc
 *       0 -> no realloc
 *       1 -> realloc
 */
static int   buffer_check(Buffer *s, size_t len);


/*
 * Lang
 */
typedef struct {
	const char *key;
	const char *value;
} Lang;

static const Lang lang_pack[] = CONFIG_LANG_PACK;

static void        lang_show_list(int max_col);
static const Lang *lang_get_from_key(const char key[]);
static int         lang_get_from_key_s(const char key[], size_t len, const Lang **lang);

/*
 * keys: source & target pairs (source_key:target_key)
 *       source / target can be empty:
 *       example:
 *           en:    -> source = en, but the target is empty
 *             :id  -> target = id, but the source is empty
 *             :    -> both empty
 *
 * return:  0 = sucess
 *         -1 = invalid keys format
 *         -2 = invalid source lang
 *         -3 = invalid target lang
 *         -4 = both are invalid
 */
static int         lang_parse(const Lang *l[2], const char keys[]);


/*
 * Net
 */
static int net_tcp_connect(const char host[], const char port[]);


/*
 * Http
 */
enum {
	HTTP_IOV_METHOD = 0,
	HTTP_IOV_PATH_BASE,
	HTTP_IOV_PATH_SPEC,
	HTTP_IOV_SRC_LANG_KEY,
	HTTP_IOV_SRC_LANG_VAL,
	HTTP_IOV_TRG_LANG_KEY,
	HTTP_IOV_TRG_LANG_VAL,
	HTTP_IOV_HL_LANG_KEY,
	HTTP_IOV_HL_LANG_VAL,
	HTTP_IOV_TEXT_KEY,
	HTTP_IOV_TEXT_VAL,
	HTTP_IOV_PROTOCOL,
	HTTP_IOV_HEADER,

	HTTP_IOVS_SIZE,
};

typedef struct {
	const char *host;
	const char *port;

	Buffer buffer;
	size_t buffer_len;

	struct iovec iovs[HTTP_IOVS_SIZE];
} Http;

static int   http_init(Http *h);
static void  http_deinit(Http *h);
static int   http_request(Http *h, int type, const char sl[], const char tl[],
			  const char hl[], const char text[]);
/* Don't free() the returned memory! */
static char *http_response_get_json(Http *h, size_t *ret_len);


/*
 * MoeTr
 */
enum {
	RESULT_TYPE_SIMPLE = 0,
	RESULT_TYPE_DETAIL,
	RESULT_TYPE_LANG,
};

const char result_type_str[][2][16] = {
	[RESULT_TYPE_SIMPLE] = { "s", "Simple" },
	[RESULT_TYPE_DETAIL] = { "d", "Detail" },
	[RESULT_TYPE_LANG]   = { "l", "Detect Language" },
};

enum {
	MOETR_INTR_CODE_NOP = 0,
	MOETR_INTR_CODE_TRANSLATE,
	MOETR_INTR_CODE_CHANGE_LANGS,
	MOETR_INTR_CODE_CHANGE_RESTYPE,
	MOETR_INTR_CODE_LANG_LIST,
	MOETR_INTR_CODE_HELP,
	MOETR_INTR_CODE_QUIT,
	MOETR_INTR_CODE_ERROR,
};

typedef struct {
	int         result_type;
	const Lang *langs[2];
	char        prompt[64];
	Http        http;
} MoeTr;

static int  moetr_init(MoeTr *m, char default_result_type, const Lang *default_langs[2]);
static void moetr_deinit(MoeTr *m);
static int  moetr_set_langs(MoeTr *m, const char keys[]);
static int  moetr_set_result_type(MoeTr *m, char type);
static int  moetr_translate(MoeTr *m, const char text[]);
static void moetr_interactive(MoeTr *m, const char text[]);


/********************************************************************************
 *                                    IMPL                                      *
 ********************************************************************************/
/*
 * cstr
 */
static size_t
cstr_trim_right(const char cstr[], size_t len)
{
	if ((len == 0) || ((len == 1) && (isspace(cstr[0]))))
		return 0;

	const char *p = cstr + (len - 1);
	while ((*p != '\0') && (p > cstr) && (isspace(*p)))
		p--;

	return (p - cstr) + 1;
}


static const char *
cstr_trim_left(const char cstr[], size_t *len)
{
	const size_t _len = *len;
	if (_len == 0)
		return cstr;

	size_t i = 0;
	while ((cstr[i] != '\0') && (isspace(cstr[i])))
		i++;

	*len = (_len - i);
	return &cstr[i];
}


static char *
cstr_trim_right_mut(char cstr[])
{
	size_t len = strlen(cstr);
	if (len == 0)
		return cstr;

	char *end = cstr + (len - 1);
	while ((*end != '\0') && (end > cstr) && (isspace(*cstr)))
		end--;

	*(end + 1) = '\0';
	return cstr;
}


static char *
cstr_trim_left_mut(char cstr[])
{
	char *start = cstr;
	while ((*start != '\0') && (isspace(*start)))
		start++;

	return start;
}


static char *
cstr_skip_html_tags(char raw[], size_t len)
{
	const struct {
		const char *const tags[2];
		size_t            len[2];

	} tag_list[] = {
		{ .tags = { "<b>", "</b>" }, .len = { 3, 4 } },
		{ .tags = { "<i>", "</i>" }, .len = { 3, 4 } },

		/* We don't really need these (probably):
		{ .tags = { "<u>", "</u>" }, .len = { 3, 4 } },
		{ .tags = { "<br>", ""    }, .len = { 4, 0 } },
		*/
	};


	const char *end = raw + (len - 1u);
	for (size_t i = 0; i < len;) {
		char *tag_close = NULL;
		for (size_t hi = 0; hi < LEN(tag_list); hi++) {
			char *tag_open = strstr(raw + i, tag_list[hi].tags[0]);
			if (tag_open == NULL)
				continue;

			tag_close = strstr(tag_open, tag_list[hi].tags[1]);
			if (tag_close == NULL)
				continue;

			memmove(tag_open, tag_open + tag_list[hi].len[0], end - tag_open);
			tag_close -= tag_list[hi].len[0];
			memmove(tag_close, tag_close + tag_list[hi].len[1], end - tag_close);
		}

		i += (end - tag_close);
	}

	return raw;
}


/*
 * Buffer
 */
static int
buffer_init(Buffer *b, size_t size)
{
	if (size >= CONFIG_BUFFER_MAX_SIZE) {
		errno = ENOMEM;
		return -1;
	}

	char *const buffer = malloc(size);
	if (buffer == NULL)
		return -1;

	b->ptr = buffer;
	b->size = size;
	return 0;
}


static void
buffer_deinit(Buffer *s)
{
	free(s->ptr);
}


static int
buffer_check(Buffer *b, size_t len)
{
	if (len < b->size)
		return 0;

	const size_t new_size = b->size + len;
	if (new_size >= CONFIG_BUFFER_MAX_SIZE) {
		errno = ENOMEM;
		return -1;
	}

	char *const new_buffer = realloc(b->ptr, new_size);
	if (new_buffer == NULL)
		return -1;

	b->ptr = new_buffer;
	b->size = new_size;
	return 1;
}


/*
 * Lang
 */
static void
lang_show_list(int max_col)
{
	const int lang_pack_len = (int)LEN(lang_pack);
	if (max_col == 0)
		max_col = 2;

	if (max_col > lang_pack_len)
		max_col = lang_pack_len;

	int col = 0;
	const int max_key_len = 6;
	const int max_val_len = 20;
	for (int i = 0; i < lang_pack_len; i++) {
		const char *const key = lang_pack[i].key;
		const char *const val = lang_pack[i].value;
		const int key_len = max_key_len - ((int)strlen(key));
		const int val_len = max_val_len - ((int)strlen(val));

		printf(COLOR_REGULAR_GREEN("[%s]")"%*s%s%*s", key, key_len, "", val, val_len, "");

		col++;
		if (col == max_col) {
			col = 0;
			putchar('\n');
		}
	}

	if (col != 0)
		putchar('\n');
}


static const Lang *
lang_get_from_key(const char key[])
{
	for (size_t i = 0; i < LEN(lang_pack); i++) {
		if (strcasecmp(lang_pack[i].key, key) == 0)
			return &lang_pack[i];
	}

	return NULL;
}


static int
lang_get_from_key_s(const char key[], size_t len, const Lang **lang)
{
	char buffer[CONFIG_LANG_KEY_SIZE + 1u];
	if (len == 0)
		return 0;

	if (len <= sizeof(buffer)) {
		memcpy(buffer, key, len);
		buffer[len] = '\0';

		const Lang *const ret_lang = lang_get_from_key(buffer);
		if (ret_lang != NULL) {
			*lang = ret_lang;
			return 0;
		}
	}

	return -1;
}


static int
lang_parse(const Lang *l[2], const char keys[])
{
	int ret = 0;
	const char *key;
	size_t key_len;


	const char *const sep = strchr(keys, ':');
	if (sep == NULL) {
		ret = -1;
		goto out0;
	}


	/* source */
	key_len = cstr_trim_right(keys, (sep - keys));
	key = cstr_trim_left(keys, &key_len);
	if (lang_get_from_key_s(key, key_len, &l[0]) < 0)
		ret -= 2;

	/* target */
	key = sep + 1;
	key_len = cstr_trim_right(key, strlen(key));
	key = cstr_trim_left(key, &key_len);
	if (key_len == 0)
		goto out0;

	if ((strncasecmp(key, "auto", key_len) == 0) || (lang_get_from_key_s(key, key_len, &l[1])) < 0)
		ret -= 3;

out0:
	if (ret == -5)
		ret = -4;

	return ret;
}


/*
 * Net
 */
static int
net_tcp_connect(const char host[], const char port[])
{
	int fd, ret;
	struct addrinfo *ai, *p = NULL;
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};


	ret = getaddrinfo(host, port, &hints, &ai);
	if (ret != 0) {
		fprintf(stderr, COLOR_REGULAR_YELLOW("net_tcp_connect: getaddrinfo: %s") "\n",
			gai_strerror(ret));
		return -1;
	}

	for (p = ai; p != NULL; p = p->ai_next) {
		fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (fd < 0) {
			perror(COLOR_REGULAR_YELLOW("net_tcp_connect: socket"));
			continue;
		}

		if (connect(fd, p->ai_addr, p->ai_addrlen) < 0) {
			perror(COLOR_REGULAR_YELLOW("net_tcp_connect: connect"));

			close(fd);
			continue;
		}

		break;
	}

	freeaddrinfo(ai);
	if (p == NULL) {
		fprintf(stderr, COLOR_REGULAR_YELLOW("net_tcp_connect: failed to connect\n"));
		return -1;
	}

	return fd;
}


/*
 * Http
 */
static int
http_init(Http *h)
{
	if (buffer_init(&h->buffer, CONFIG_BUFFER_SIZE * 3) < 0) {
		perror(COLOR_REGULAR_YELLOW("http_init: str_init"));
		return -1;
	}

	h->buffer_len = 0;

	h->host = CONFIG_HTTP_HOST;
	h->port = CONFIG_HTTP_PORT;

	h->iovs[HTTP_IOV_METHOD].iov_base    = CONFIG_HTTP_METHOD;
	h->iovs[HTTP_IOV_METHOD].iov_len     = sizeof(CONFIG_HTTP_METHOD) - 1;
	h->iovs[HTTP_IOV_PATH_BASE].iov_base = CONFIG_HTTP_PATH_BASE;
	h->iovs[HTTP_IOV_PATH_BASE].iov_len  = sizeof(CONFIG_HTTP_PATH_BASE) - 1;
	h->iovs[HTTP_IOV_PROTOCOL].iov_base  = CONFIG_HTTP_PROTOCOL;
	h->iovs[HTTP_IOV_PROTOCOL].iov_len   = sizeof(CONFIG_HTTP_PROTOCOL) - 1;
	h->iovs[HTTP_IOV_HEADER].iov_base    = CONFIG_HTTP_HEADER;
	h->iovs[HTTP_IOV_HEADER].iov_len     = sizeof(CONFIG_HTTP_HEADER) - 1;
	return 0;
}


static void
http_deinit(Http *h)
{
	buffer_deinit(&h->buffer);
}


static const char *
__http_url_encode(Http *h, const char plain[])
{
	const size_t plain_len = strlen(plain);
	if (plain_len == 0)
		return NULL;

	if (buffer_check(&h->buffer, (plain_len * 3) + 1) < 0)
		return NULL;

	char *const buffer = h->buffer.ptr;
	const size_t buffer_size = h->buffer.size;

	const char *const hex  = "0123456789abcdef";
	const unsigned char *p = (const unsigned char *)plain;

	size_t i = 0, pos = 0;
	while ((p[i] != '\0') && (i < buffer_size || pos < buffer_size) && (i < plain_len)) {
		if (!isalnum(p[i])) {
			buffer[pos++] = '%';
			buffer[pos++] = hex[(p[i] >> 4u) & 15u];
			buffer[pos++] = hex[p[i] & 15u];

			i++;
			continue;
		}

		buffer[pos++] = p[i++];
	}

	h->buffer_len = pos;
	buffer[pos] = '\0';
	return buffer;
}


static void
__http_build_request(Http *h, int type, const char sl[], const char tl[], const char hl[],
		     const char text[], size_t text_len)
{
	/* HEHE... :D */
	/*
	 * iovs[0]  = METHOD
	 * iovs[1]  = path base
	 * iovs[2]  = specific path
	 * iovs[3]  = source lang key
	 * iovs[4]  = source lang val
	 * iovs[5]  = target lang key
	 * iovs[6]  = target lang val
	 * iovs[7]  = highlight lang key
	 * iovs[8]  = highlight lang val
	 * iovs[9]  = text key
	 * iovs[10] = text val
	 * iovs[11] = PROTOCOL
	 * iovs[12] = header
	 */

	h->iovs[HTTP_IOV_PATH_SPEC].iov_len    = 0;
	h->iovs[HTTP_IOV_SRC_LANG_KEY].iov_len = 0;
	h->iovs[HTTP_IOV_SRC_LANG_VAL].iov_len = 0;
	h->iovs[HTTP_IOV_TRG_LANG_KEY].iov_len = 0;
	h->iovs[HTTP_IOV_TRG_LANG_VAL].iov_len = 0;
	h->iovs[HTTP_IOV_HL_LANG_KEY].iov_len  = 0;
	h->iovs[HTTP_IOV_HL_LANG_VAL].iov_len  = 0;


	h->iovs[HTTP_IOV_TEXT_KEY].iov_base = CONFIG_HTTP_QUERY_TXT;
	h->iovs[HTTP_IOV_TEXT_KEY].iov_len  = sizeof(CONFIG_HTTP_QUERY_TXT) - 1;
	h->iovs[HTTP_IOV_TEXT_VAL].iov_base = (char *)text;
	h->iovs[HTTP_IOV_TEXT_VAL].iov_len  = text_len;


	switch (type) {
	case RESULT_TYPE_LANG:
		h->iovs[HTTP_IOV_PATH_SPEC].iov_base = CONFIG_HTTP_PATH_LANG;
		h->iovs[HTTP_IOV_PATH_SPEC].iov_len  = sizeof(CONFIG_HTTP_PATH_LANG) - 1;
		break;
	case RESULT_TYPE_DETAIL:
		h->iovs[HTTP_IOV_PATH_SPEC].iov_base = CONFIG_HTTP_PATH_DETAIL;
		h->iovs[HTTP_IOV_PATH_SPEC].iov_len  = sizeof(CONFIG_HTTP_PATH_DETAIL) - 1;

		h->iovs[HTTP_IOV_HL_LANG_KEY].iov_base = CONFIG_HTTP_QUERY_HL;
		h->iovs[HTTP_IOV_HL_LANG_KEY].iov_len  = sizeof(CONFIG_HTTP_QUERY_HL) - 1;
		h->iovs[HTTP_IOV_HL_LANG_VAL].iov_base = (char *)hl;
		h->iovs[HTTP_IOV_HL_LANG_VAL].iov_len  = strlen(hl);

		/* FALLTHROUGH */
	case RESULT_TYPE_SIMPLE:
		if (type == RESULT_TYPE_SIMPLE) {
			h->iovs[HTTP_IOV_PATH_SPEC].iov_base = CONFIG_HTTP_PATH_SIMPLE;
			h->iovs[HTTP_IOV_PATH_SPEC].iov_len  = sizeof(CONFIG_HTTP_PATH_SIMPLE) - 1;
		}

		h->iovs[HTTP_IOV_SRC_LANG_KEY].iov_base = CONFIG_HTTP_QUERY_SL;
		h->iovs[HTTP_IOV_SRC_LANG_KEY].iov_len  = sizeof(CONFIG_HTTP_QUERY_SL) - 1;
		h->iovs[HTTP_IOV_SRC_LANG_VAL].iov_base = (char *)sl;
		h->iovs[HTTP_IOV_SRC_LANG_VAL].iov_len  = strlen(sl);

		h->iovs[HTTP_IOV_TRG_LANG_KEY].iov_base = CONFIG_HTTP_QUERY_TL;
		h->iovs[HTTP_IOV_TRG_LANG_KEY].iov_len  = sizeof(CONFIG_HTTP_QUERY_TL) - 1;
		h->iovs[HTTP_IOV_TRG_LANG_VAL].iov_base = (char *)tl;
		h->iovs[HTTP_IOV_TRG_LANG_VAL].iov_len  = strlen(tl);
		break;
	}
}


static int
http_request(Http *h, int type, const char sl[], const char tl[], const char hl[], const char text[])
{
	const char *const text_enc = __http_url_encode(h, text);
	if (text_enc == NULL)
		return -1;

	const int fd = net_tcp_connect(h->host, h->port);
	if (fd < 0)
		return -1;

	__http_build_request(h, type, sl, tl, hl, text_enc, h->buffer_len);

	const ssize_t written = writev(fd, h->iovs, HTTP_IOVS_SIZE);
	if (written < 0) {
		perror(COLOR_REGULAR_YELLOW("http_request: writev"));
		goto err0;
	}

	size_t total_len = 0;
	for (size_t i = 0; i < LEN(h->iovs); i++)
		total_len += h->iovs[i].iov_len;

	if (total_len != (size_t)written) {
		fprintf(stderr, COLOR_REGULAR_YELLOW("http_request: writev: incomplete: [%zu:%zu]") "\n",
			written, total_len);
		goto err0;
	}


	char *buffer = h->buffer.ptr;
	size_t buffer_len = h->buffer.size;
	size_t recvd = 0;
	while (1) {
		const ssize_t rv = recv(fd, buffer + recvd, buffer_len - recvd, 0);
		if (rv < 0) {
			perror(COLOR_REGULAR_YELLOW("http_request: recv"));
			goto err0;
		}

		if (rv == 0)
			break;

		recvd += (size_t)rv;
		switch (buffer_check(&h->buffer, recvd + 1)) {
		case 0:
			break;
		case 1:
			buffer = h->buffer.ptr;
			buffer_len = h->buffer.size;
			break;
		case -1:
			goto err0;
		}
	}

	h->buffer_len = recvd;
	buffer[recvd] = '\0';
	close(fd);
	return 0;

err0:
	close(fd);
	return -1;
}


static char *
http_response_get_json(Http *h, size_t *ret_len)
{
	char *const buffer = h->buffer.ptr;
	if (h->buffer_len == 0)
		goto err0;

	char *const first = strstr(buffer, "\r\n");
	if (first == NULL)
		goto err0;

	*first = '\0';
	if (strstr(buffer, "200") == NULL)
		goto err0;

	char *const end = strstr(first + 2, "\r\n\r\n");
	if (end == NULL)
		goto err0;

	char *const json_start = strchr(end + 4, '[');
	if (json_start == NULL)
		goto err0;

	char *const json_end = strrchr(json_start + 1, ']');
	if (json_end == NULL)
		goto err0;

	const size_t len = ((json_end + 1) - json_start);
	json_end[len] = '\0';

	*ret_len = len;
	return json_start;

err0:
	fprintf(stderr, COLOR_REGULAR_YELLOW("http_response_get_json: invalid reponse") "\n");
	return NULL;
}


/*
 * MoeTr
 */
static int
moetr_init(MoeTr *m, char default_result_type, const Lang *default_langs[2])
{
	memset(m, 0, sizeof(*m));
	m->langs[0] = default_langs[0];
	m->langs[1] = default_langs[1];

	if (moetr_set_result_type(m, default_result_type) < 0)
		return -1;

	if (http_init(&m->http) < 0)
		return -1;

	return 0;
}


static void
moetr_deinit(MoeTr *m)
{
	http_deinit(&m->http);
}


static int
moetr_set_langs(MoeTr *m, const char keys[])
{
	const int ret = lang_parse(m->langs, keys);
	switch (ret) {
	case -1:
		fprintf(stderr, COLOR_REGULAR_YELLOW("moetr_set_langs: invalid keys format") "\n");
		break;
	case -2:
		fprintf(stderr, COLOR_REGULAR_YELLOW("moetr_set_langs: invalid source lang") "\n");
		break;
	case -3:
		fprintf(stderr, COLOR_REGULAR_YELLOW("moetr_set_langs: invalid target lang") "\n");
		break;
	case -4:
		fprintf(stderr, COLOR_REGULAR_YELLOW("moetr_set_langs: invalid source and target langs") "\n");
		break;
	}

	return ret;
}


static int
moetr_set_result_type(MoeTr *m, char type)
{
	switch (tolower(type)) {
	case 's':
		m->result_type = RESULT_TYPE_SIMPLE;
		break;
	case 'd':
		m->result_type = RESULT_TYPE_DETAIL;
		break;
	case 'l':
		m->result_type = RESULT_TYPE_LANG;
		break;
	default:
		fprintf(stderr, COLOR_REGULAR_YELLOW("moetr_set_result_type: invalid result type") "\n");
		return -1;
	}

	return 0;
}


static json_value_t *
__json_array_index(json_array_t *arr, size_t index)
{
	if (arr != NULL) {
		size_t i = 0;
		for (json_array_element_t *e = arr->start; e != NULL; e = e->next) {
			if (i == index)
				return e->value;

			i++;
		}
	}

	return NULL;
}


static json_array_t *
__json_value_as_array(json_value_t *val)
{
	if (val != NULL)
		return json_value_as_array(val);

	return NULL;
}


static json_string_t *
__json_value_as_string(json_value_t *val)
{
	if (val != NULL)
		return json_value_as_string(val);

	return NULL;
}


static size_t
__json_array_fills(json_value_t *values[], size_t size, json_array_t *arr)
{
	size_t i = 0;
	if (arr == NULL)
		goto clear0;

	for (json_array_element_t *e = arr->start; e != NULL; e = e->next)
		values[i++] = e->value;

clear0:
	for (size_t j = i; j < size; j++)
		values[j] = NULL;

	return i;
}


static void
__moetr_print_simple(json_value_t *json)
{
	json_array_t *arr = json_value_as_array(json);
	if (arr == NULL)
		return;

	arr = __json_value_as_array(__json_array_index(arr, 0));
	if (arr == NULL)
		return;

	for (json_array_element_t *e = arr->start; e != NULL; e = e->next) {
		arr = json_value_as_array(e->value);
		if (arr == NULL)
			continue;

		json_string_t *const str = __json_value_as_string(__json_array_index(arr, 0));
		if (str == NULL)
			continue;

		printf("%.*s", (int)str->string_size, str->string);
	}

	putchar('\n');
}


static void
__moetr_print_detail_synonyms(const json_array_t *synonyms_a)
{
	json_array_t *arr;
	json_string_t *str;
	json_value_t *values[3];


	printf("\n------------------------");
	for (json_array_element_t *e = synonyms_a->start; e != NULL; e = e->next) {
		arr = json_value_as_array(e->value);
		if (arr == NULL)
			continue;

		if (__json_array_fills(values, LEN(values), arr) == 0)
			continue;

		/* verbs, nouns, etc. */
		str = __json_value_as_string(values[0]);
		if (str != NULL) {
			/* no label */
			if (str->string_size == 0) {
				printf("\n" COLOR_BOLD_BLUE("[?]"));
			} else {
				printf("\n" COLOR_BOLD_BLUE("[%c%.*s]"), toupper(str->string[0]),
				       (int)str->string_size - 1, &str->string[1]);
			}
		}


		/* target alternative(s) */
		arr = __json_value_as_array(values[2]);
		if (arr == NULL)
			continue;

		int iter = 1;
		for (json_array_element_t *ee = arr->start; ee != NULL; ee = ee->next) {
			if (__json_array_fills(values, LEN(values), json_value_as_array(ee->value)) == 0)
				continue;

			str = __json_value_as_string(values[0]);
			if (str == NULL)
				continue;

			printf("\n" COLOR_BOLD_WHITE("%d. %c%.*s:") "\n   "
			       COLOR_REGULAR_YELLOW("-> "), iter, toupper(str->string[0]),
			       (int)str->string_size, &str->string[1]);

			/* source alternatives */
			arr = __json_value_as_array(values[1]);
			if (arr == NULL)
				continue;

			int _len = (int)arr->length;
			for (json_array_element_t *eee = arr->start; eee != NULL; eee = eee->next) {
				str = json_value_as_string(eee->value);
				if (str == NULL)
					continue;

				printf("%.*s", (int)str->string_size, str->string);

				if (_len-- > 1)
					printf(", ");
			}

			if (_len == 0)
				printf(".");

			if (iter == CONFIG_SYN_LINES_MAX)
				break;

			iter++;
		}
		putchar('\n');
	}
	putchar('\n');
}


static void
__moetr_print_detail_defs(const json_array_t *defs_a)
{
	json_array_t *arr;
	json_string_t *str;
	json_value_t *values[4];


	printf("\n------------------------");
	for (json_array_element_t *e = defs_a->start; e != NULL; e = e->next) {
		arr = json_value_as_array(e->value);
		if (arr == NULL)
			continue;

		if (__json_array_fills(values, LEN(values), arr) == 0)
			continue;

		/* verbs, nouns, etc. */
		str = __json_value_as_string(values[0]);
		if (str != NULL) {
			/* no label */
			if (str->string_size == 0) {
				printf("\n" COLOR_BOLD_YELLOW("[?]"));
			} else {
				printf("\n" COLOR_BOLD_YELLOW("[%c%.*s]"), toupper(str->string[0]),
				       (int)str->string_size - 1, &str->string[1]);
			}
		}

		arr = __json_value_as_array(values[1]);
		if (arr == NULL)
			continue;

		int iter = 1;
		for (json_array_element_t *ee = arr->start; ee != NULL; ee = ee->next) {
			if (__json_array_fills(values, LEN(values), json_value_as_array(ee->value)) == 0)
				continue;

			str = __json_value_as_string(values[0]);
			if (str == NULL)
				continue;

			printf("\n" COLOR_BOLD_WHITE("%d. %c%.*s"), iter, toupper(str->string[0]),
			       (int)str->string_size, &str->string[1]);

			json_array_t *_arr = __json_value_as_array(values[3]);
			if (_arr != NULL) {
				_arr = __json_value_as_array(__json_array_index(_arr, 0));
				if ((_arr != NULL) && (_arr->length > 0)) {
					str = json_value_as_string(_arr->start->value);
					if (str != NULL) {
						printf(COLOR_REGULAR_GREEN(" [%.*s] "),
						       (int)str->string_size, str->string);
					}
				}
			}

			str = __json_value_as_string(values[2]);
			if (str != NULL) {
				printf("\n" COLOR_REGULAR_YELLOW("   ->") " %c%.*s.",
				       toupper(str->string[0]), (int)str->string_size,
				       &str->string[1]);
			}

			if (iter == CONFIG_DEF_LINES_MAX)
				break;

			iter++;
		}
		putchar('\n');
	}
	putchar('\n');
}


static void
__moetr_print_detail_examples(json_array_t *examples_a)
{
	char buffer[CONFIG_EXM_BUFFER_SIZE];


	printf("\n------------------------\n");
	for (json_array_element_t *e = examples_a->start; e != NULL; e = e->next) {
		json_array_t *arr = json_value_as_array(e->value);
		if (arr == NULL)
			continue;

		int iter = 1;
		for (json_array_element_t *ee = arr->start; ee != NULL; ee = ee->next) {
			arr = json_value_as_array(ee->value);
			if (arr == NULL)
				continue;

			json_string_t *const str = __json_value_as_string(__json_array_index(arr, 0));
			if (str == NULL)
				continue;

			const size_t len = str->string_size;
			if (len >= sizeof(buffer))
				continue;

			memcpy(buffer, str->string, len);
			buffer[len] = '\0';

			char *const res = cstr_skip_html_tags(buffer, len);
			res[0] = toupper(res[0]);

			printf("%d. " COLOR_REGULAR_YELLOW("%s.") "\n", iter, res);

			if (iter == CONFIG_EXM_LINES_MAX)
				break;

			iter++;
		}

		putchar('\n');
	}
}


static void
__moetr_print_detail(const MoeTr *m, json_value_t *json, const char src_text[])
{
	json_array_t *const root_a = json_value_as_array(json);
	if (root_a == NULL)
		return;

	json_value_t *root_v[14];
	if (__json_array_fills(root_v, LEN(root_v), root_a) == 0)
		return;


	/* bufferred print */
	char buffer[CONFIG_PRINT_BUFFER_SIZE];
	const int buffer_set = setvbuf(stdout, buffer, _IOFBF, CONFIG_PRINT_BUFFER_SIZE);
	/* bufferred print */


	json_array_t *const text_a = __json_value_as_array(root_v[0]);
	json_value_t *splls_v[4];


	json_array_t *splls_a = NULL;
	if (text_a->length > 1)
		splls_a = __json_value_as_array(__json_array_index(text_a, (text_a->length - 1)));

	__json_array_fills(splls_v, LEN(splls_v), splls_a);


	/* source: correction */
	json_array_t *const src_cor_a = __json_value_as_array(root_v[7]);
	json_string_t *const src_cor_s = __json_value_as_string(__json_array_index(src_cor_a, 1));
	if (src_cor_s != NULL) {
		printf(COLOR_BOLD_GREEN("Did you mean: ") "\"%.*s\" " COLOR_BOLD_GREEN("?") "\n\n",
		       (int)src_cor_s->string_size, src_cor_s->string);
	}


	/* source: text */
	printf(COLOR_REGULAR_YELLOW("%s") "\n", src_text);


	/* source: spelling */
	json_string_t *const src_splls_s = __json_value_as_string(splls_v[3]);
	if (src_splls_s != NULL) {
		printf("(" COLOR_REGULAR_GREEN("%.*s") ")\n", (int)src_splls_s->string_size,
		       src_splls_s->string);
	}


	/* source: language */
	json_string_t *const src_lang_s = __json_value_as_string(root_v[2]);
	if ((src_lang_s != NULL) && (strcasecmp("auto", m->langs[0]->key) == 0)) {
		const Lang *lang = NULL;
		const char *lang_val = "Unknown";
		if (lang_get_from_key_s(src_lang_s->string, src_lang_s->string_size, &lang) == 0)
			lang_val = lang->value;

		printf(COLOR_BOLD_GREEN("[%.*s]:") COLOR_BOLD_WHITE(" %s") "\n",
		       (int)src_lang_s->string_size, src_lang_s->string, lang_val);
	}
	printf("\n------------------------\n");


	/* target: text */
	if (text_a != NULL) {
		for (json_array_element_t *e = text_a->start; e != NULL; e = e->next) {
			json_array_t *const arr = json_value_as_array(e->value);
			if (arr == NULL)
				continue;

			json_string_t *const str = __json_value_as_string(__json_array_index(arr, 0));
			if (str == NULL)
				continue;

			printf("%.*s", (int)str->string_size, str->string);
		}

		putchar('\n');
	}


	/* target: spelling */
	json_string_t *const trg_splls_s = __json_value_as_string(splls_v[2]);
	if (trg_splls_s != NULL) {
		printf("( " COLOR_REGULAR_GREEN("%.*s") " )\n", (int)trg_splls_s->string_size,
		       trg_splls_s->string);
	}


	/* synonyms */
	json_array_t *const synonyms_a = __json_value_as_array(root_v[1]);
	if ((synonyms_a != NULL) && (CONFIG_SYN_LINES_MAX != 0))
		__moetr_print_detail_synonyms(synonyms_a);


	/* definitions */
	json_array_t *const defs_a = __json_value_as_array(root_v[12]);
	if ((defs_a != NULL) && (CONFIG_DEF_LINES_MAX != 0))
		__moetr_print_detail_defs(defs_a);


	/* examples */
	json_array_t *const examples_a = __json_value_as_array(root_v[13]);
	if ((examples_a != NULL) && (CONFIG_EXM_LINES_MAX != 0))
		__moetr_print_detail_examples(examples_a);


	/* bufferred print */
	if (buffer_set == 0) {
		fflush(stdout);
		setvbuf(stdout, NULL, _IOLBF, 0);
	}
	/* bufferred print */
}


static void
__moetr_print_detect_lang(json_value_t *json)
{
	json_array_t *const arr = json_value_as_array(json);
	if (arr == NULL)
		return;

	json_string_t *const str = __json_value_as_string(__json_array_index(arr, 2));
	if (str == NULL)
		return;

	const Lang *lang = NULL;
	const char *lang_val = "Unknown";
	if (lang_get_from_key_s(str->string, str->string_size, &lang) == 0)
		lang_val = lang->value;

	printf("%.*s (%s)\n", (int)str->string_size, str->string, lang_val);
}


static int
moetr_translate(MoeTr *m, const char text[])
{
	const char *const src = m->langs[0]->key;
	const char *const trg = m->langs[1]->key;
	if (http_request(&m->http, m->result_type, src, trg, trg, text) < 0)
		return -1;

	size_t len;
	char *const res = http_response_get_json(&m->http, &len);
	if (res == NULL)
		return -1;

	json_value_t *const json = json_parse(res, len);
	if (json == NULL) {
		fprintf(stderr, COLOR_REGULAR_YELLOW("moetr_translate: json_parse: failed to parse") "\n");
		return -1;
	}

	switch (m->result_type) {
	case RESULT_TYPE_SIMPLE:
		__moetr_print_simple(json);
		break;
	case RESULT_TYPE_DETAIL:
		__moetr_print_detail(m, json, text);
		break;
	case RESULT_TYPE_LANG:
		__moetr_print_detect_lang(json);
		break;
	}

	free(json);
	return 0;
}


static void
__moetr_interactive_banner(const MoeTr *m)
{
	printf(COLOR_BOLD_WHITE("---[ Moetranslate ]---") "\n"
	       COLOR_BOLD_GREEN("Languages:         ") "%s (%s) -> %s (%s)\n"
	       COLOR_BOLD_GREEN("Result type:       ") "%s (%s)\n"
	       COLOR_BOLD_GREEN("Show command list: ") "Type '/' and [Enter]\n\n",
	       m->langs[0]->value, m->langs[0]->key, m->langs[1]->value, m->langs[1]->key,
	       result_type_str[m->result_type][0], result_type_str[m->result_type][1]);
}


static void
__moetr_interactive_help(void)
{
	printf(COLOR_BOLD_GREEN("Change languages: ") COLOR_REGULAR_YELLOW("/c") " [SOURCE]:[TARGET]\n"
	       COLOR_BOLD_GREEN("Result type:      ") COLOR_REGULAR_YELLOW("/r") " [TYPE]\n"
	                        "                      %s = %s\n"
	                        "                      %s = %s\n"
	                        "                      %s = %s\n"
	       COLOR_BOLD_GREEN("Show languages:   ") COLOR_REGULAR_YELLOW("/l") " [NUM]\n"
	       COLOR_BOLD_GREEN("Quit:             ") COLOR_REGULAR_YELLOW("/q") "\n\n",
	       result_type_str[RESULT_TYPE_SIMPLE][0],
	       result_type_str[RESULT_TYPE_SIMPLE][1],
	       result_type_str[RESULT_TYPE_DETAIL][0],
	       result_type_str[RESULT_TYPE_DETAIL][1],
	       result_type_str[RESULT_TYPE_LANG][0],
	       result_type_str[RESULT_TYPE_LANG][1]);
}


static int
__moetr_interactive_parse(char *cmd[])
{
	char *_cmd = *cmd;
	if (*_cmd == '\0')
		return MOETR_INTR_CODE_NOP;

	/*
	 * escape '/' character by adding '\' prefix:
	 * example: \/q
	 */
	if ((*_cmd == '\\') && (*(_cmd + 1) == '/')) {
		*cmd = _cmd + 1;
		return MOETR_INTR_CODE_TRANSLATE;
	}

	if (*(_cmd++) != '/')
		return MOETR_INTR_CODE_TRANSLATE;

	switch (tolower(*_cmd)) {
	case 'c':
		*cmd = _cmd + 1;
		return MOETR_INTR_CODE_CHANGE_LANGS;
	case 'r':
		_cmd = cstr_trim_right_mut(cstr_trim_left_mut(_cmd + 1));
		if (strlen(_cmd) > 1)
			break;

		*cmd = _cmd;
		return MOETR_INTR_CODE_CHANGE_RESTYPE;
	case 'l':
		if (*(_cmd + 1) == '\0')
			*cmd = "2";
		else
			*cmd = _cmd + 1;

		return MOETR_INTR_CODE_LANG_LIST;
	case '\0':
		return MOETR_INTR_CODE_HELP;
	case 'q':
		return MOETR_INTR_CODE_QUIT;
	}

	return MOETR_INTR_CODE_ERROR;
}


static void
__moetr_set_prompt(MoeTr *m)
{
	snprintf(m->prompt, sizeof(m->prompt), COLOR_BOLD_WHITE("[%s:%s][%s]->") " ",
		 m->langs[0]->key, m->langs[1]->key, result_type_str[m->result_type][0]);
}


static void
moetr_interactive(MoeTr *m, const char text[])
{
	setlocale(LC_CTYPE, "");
	stifle_history(CONFIG_INTERACTIVE_HISTORY_SIZE);
	__moetr_set_prompt(m);
	__moetr_interactive_banner(m);

	if (text != NULL)
		moetr_translate(m, text);

	int is_alive = 1;
	while (is_alive) {
		char *const res = readline(m->prompt);
		if (res == NULL)
			return;

		char *cmd = cstr_trim_right_mut(cstr_trim_left_mut(res));
		switch (__moetr_interactive_parse(&cmd)) {
		case MOETR_INTR_CODE_TRANSLATE:
			puts("------------------------");
			moetr_translate(m, cmd);
			puts("------------------------");
			break;
		case MOETR_INTR_CODE_CHANGE_LANGS:
			if (moetr_set_langs(m, cmd) == 0)
				__moetr_set_prompt(m);

			cmd = res;
			break;
		case MOETR_INTR_CODE_CHANGE_RESTYPE:
			if (moetr_set_result_type(m, *cmd) == 0)
				__moetr_set_prompt(m);

			cmd = res;
			break;
		case MOETR_INTR_CODE_NOP:
			break;
		case MOETR_INTR_CODE_LANG_LIST:
			lang_show_list(atoi(cmd));
			cmd = res;
			break;
		case MOETR_INTR_CODE_HELP:
			__moetr_interactive_help();
			break;
		case MOETR_INTR_CODE_QUIT:
			is_alive = 0;
			break;
		case MOETR_INTR_CODE_ERROR:
		default:
			puts("Invalid command!");
		}

		if (*cmd != '\0')
			add_history(cmd);

		free(res);
	}
}


/*
 * Main
 */
static void
__help(const char name[])
{
	printf("%s - A simple language translator\n\n"
		"Usage: moetranslate -[s/d/l/i/L/h] [SOURCE:TARGET] [TEXT]\n"
		"   -s            Simple mode\n"
		"   -d            Detail mode\n"
		"   -l            Detect language\n"
		"   -L            Language list\n"
		"   -i            Interactive mode\n"
		"   -h            Show help\n\n"
		"Examples:\n"
		"   Simple Mode:   %s -s en:id \"Hello world\"\n"
		"   Detail Mode:   %s -d id:en Halo\n"
		"   Auto Lang:     %s -d auto:en こんにちは\n"
		"   Detect Lang:   %s -l 你好\n"
		"   Language list: %s -L [NUM]\n"
		"   Interactive:   %s -i\n"
		"                  %s -i -d auto:en\n"
		"                  %s -i -d :en hello\n",
		name, name, name, name, name, name, name, name, name
	);
}


static void
__load_default(char *type, const Lang *langs[2])
{
	if (CONFIG_BUFFER_SIZE > CONFIG_BUFFER_MAX_SIZE) {
		fprintf(stderr, COLOR_REGULAR_YELLOW("config: invalid buffer size!") "\n");
		exit(1);
	}

	if ((CONFIG_LANG_INDEX_SRC < 0) || (CONFIG_LANG_INDEX_SRC > (LEN(lang_pack) - 1))) {
		fprintf(stderr, COLOR_REGULAR_YELLOW("config: invalid source lang!") "\n");
		exit(1);
	}

	if ((CONFIG_LANG_INDEX_TRG <= 0) || (CONFIG_LANG_INDEX_TRG > (LEN(lang_pack) - 1))) {
		fprintf(stderr, COLOR_REGULAR_YELLOW("config: invalid target lang!") "\n");
		exit(1);
	}

	if ((CONFIG_RESULT_TYPE < RESULT_TYPE_SIMPLE) || (CONFIG_RESULT_TYPE > RESULT_TYPE_LANG)) {
		fprintf(stderr, COLOR_REGULAR_YELLOW("config: invalid result type!") "\n");
		exit(1);
	}

	langs[0] = &lang_pack[CONFIG_LANG_INDEX_SRC];
	langs[1] = &lang_pack[CONFIG_LANG_INDEX_TRG];
	*type = result_type_str[CONFIG_RESULT_TYPE][0][0];
}


int
main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	int lang_list_max_col = 0;
	int is_interactive = 0;
	int is_detect_lang = 0;
	char result_type;
	char *text = NULL;
	const Lang *langs[2];
	MoeTr moe;


	__load_default(&result_type, langs);
	if (moetr_init(&moe, result_type, langs) < 0)
		return ret;

	int opt;
	while ((opt = getopt(argc, argv, "s:d:l:iLh")) != -1) {
		switch (opt) {
		case 's':
			moetr_set_result_type(&moe, 's');
			if (moetr_set_langs(&moe, cstr_trim_left_mut(argv[optind - 1])) < 0)
				goto out0;
			break;
		case 'd':
			moetr_set_result_type(&moe, 'd');
			if (moetr_set_langs(&moe, cstr_trim_left_mut(argv[optind - 1])) < 0)
				goto out0;
			break;
		case 'l':
			moetr_set_result_type(&moe, 'l');
			is_detect_lang = 1;
			break;
		case 'i':
			is_interactive = 1;
			break;
		case 'L':
			if (optind < argc) {
				if (argv[optind][0] == '-')
					lang_list_max_col = atoi(&argv[optind][2]);
				else
					lang_list_max_col = atoi(argv[optind]);
			}

			lang_show_list(lang_list_max_col);
			ret = EXIT_SUCCESS;
			goto out0;
		case 'h':
			__help(argv[0]);
			ret = EXIT_SUCCESS;
			goto out0;
		default:
			goto out0;
		}
	}


	if (is_detect_lang)
		text = cstr_trim_right_mut(cstr_trim_left_mut(argv[optind - 1]));
	else if (optind < argc)
		text = cstr_trim_right_mut(cstr_trim_left_mut(argv[optind]));

	if (is_interactive) {
		moetr_interactive(&moe, text);
	} else if (text != NULL) {
		if (moetr_translate(&moe, text) < 0)
			goto out1;
	} else {
		goto out0;
	}

	ret = EXIT_SUCCESS;

out0:
	if (ret == EXIT_FAILURE) {
		fprintf(stderr, COLOR_REGULAR_YELLOW("Error: invalid argument!") "\n\n");
		__help(argv[0]);
	}

out1:
	moetr_deinit(&moe);
	return ret;
}

