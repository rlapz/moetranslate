#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "jsmn.h"

/* macros */

enum {
	BRIEF,
	FULL
};

typedef struct {
	char *src;
	char *dest;
	char *text;
} Lang;

typedef struct {
	char *memory;
	size_t size;
} Memory;

/* function declaration */
static void brief_mode(void);
static void full_mode(void);
static char *url_parser(CURL *curl, int mode);
static char *request_handler(CURL *curl, const char *url);
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *data);

/* variables */
static Lang lang;
static const char url_google[] = "https://translate.google.com/translate_a/single?";

static const char *url_params[] = {
	[BRIEF]	= "client=gtx&sl=%s&tl=%s&dt=t&q=%s",
	[FULL]	= "client=gtx&ie=UTF-8&oe=UTF-8&dt=bd&dt=x&dt=ld&dt=md&dt=rw&"
		  "dt=rm&dt=ss&dt=t&dt=at&dt=gt&dt=qc&sl=%s&tl=%s&hl=id&q=%s"
};


/* function implementations */
static void
brief_mode(void)
{
	char *url;
	char *dest;
	jsmn_parser parser;
	jsmntok_t tok[128];
	jsmntok_t *t;
	int r, tok_pos = 3;

	CURL *curl;
	url = url_parser(curl, BRIEF);

	/* init curl session */
	curl = curl_easy_init();
	dest = request_handler(curl, url);

	jsmn_init(&parser);
	r = jsmn_parse(&parser, dest, strlen(dest), tok,
			sizeof(tok) / sizeof(tok[0]));
	if (r < 0)
		return;


	t = &tok[tok_pos];
	fprintf(stdout, "%.*s\n", t->end - t->start, dest + t->start);
	

	free(dest);
	free(url);
	curl_easy_cleanup(curl);
}

static void
full_mode(void)
{
	char *url;
	char *dest;

	CURL *curl;
	url = url_parser(curl, FULL);

	/* init curl session */
	curl = curl_easy_init();
	dest = request_handler(curl, url);
	/* TODO */

	puts(dest);
	free(dest);
	free(url);
	curl_easy_cleanup(curl);
}

static char *url_parser(CURL *curl, int mode)
{
	char *ret;
	char *tmp;
	char *curl_escape;
	size_t len_ret;

	/* create duplicate variable url_google */
	ret = strdup(url_google);
	/* we want appending url_google with url_params */
	tmp = realloc(ret, strlen(ret)+strlen(url_params[mode])+1);
	ret = tmp;
	strcat(ret, url_params[mode]);

	/* create url escape */
	curl_escape = curl_easy_escape(curl, lang.text, strlen(lang.text));

	len_ret = strlen(ret);
	tmp = realloc(ret, len_ret +
			strlen(curl_escape) +
			strlen(lang.src) + strlen(lang.dest)+1);
	ret = tmp;
	tmp = strdup(ret);
	sprintf(ret, tmp, lang.src, lang.dest, curl_escape);

	free(tmp);
	curl_free(curl_escape);
	return ret;
}

static char *
request_handler(CURL *curl, const char *url)
{
	Memory mem;
	CURLcode ccode;

	mem.memory = malloc(1);
	mem.size = 0;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&mem);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	ccode = curl_easy_perform(curl);

	return mem.memory;
}

static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	Memory *mem = (Memory*)data;

	char *ptr = realloc(mem->memory, mem->size + realsize +1);
	if (!ptr) {
		return 1;
	}

	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}


int
main(int argc, char *argv[])
{
	if (argc < 4) {
		fprintf(stderr, "%s SOURCE TARGET [-b] TEXT\n"
				"Example:\n"
				"\t%s en id -b \"hello\"\n",
				argv[0], argv[0]);
		return 1;
	}

	lang.src = argv[1];
	lang.dest = argv[2];

	if (strcmp(argv[3], "-b") == 0) {
		lang.text = argv[4];
		brief_mode();
	} else {
		lang.text = argv[3];
		full_mode();
	}

	return 0;
}
