#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "cJSON.h"


/* macros */
#define SUM_LEN_STRING(A, B, C, D) \
	(strlen(A) + strlen(B) + strlen(C) + strlen(D))

enum {
	BRIEF,	/* brief mode */
	FULL	/* full mode */
};

typedef struct {
	char *src;	/* source language */
	char *dest;	/* target language */
	char *text;	/* text/words */
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

/* global variables */
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
	char *url	= NULL;
	char *dest	= NULL;
	CURL *curl	= NULL;
	cJSON *parser, *array, *iterator, *value;

	url = url_parser(curl, BRIEF);
	if (!url)
		return;
	
	/* init curl session */
	curl = curl_easy_init();
	if (!curl)
		goto cleanup;
	
	dest = request_handler(curl, url);
	if (!dest)
		goto cleanup;


	/* JSON parser */
	/* dest[i][0][0] */
	parser = cJSON_Parse(dest);
	array = cJSON_GetArrayItem(parser, 0);

	cJSON_ArrayForEach(iterator, array) {
		value = cJSON_GetArrayItem(iterator, 0);
		if (cJSON_IsString(value))
			/* show the result to stdout */
			fprintf(stdout, "%s", cJSON_GetStringValue(value));
	}
	puts("");

	cJSON_Delete(parser);

cleanup:
	if (dest)
		free(dest);
	if (url)
		free(url);
	if (curl)
		curl_easy_cleanup(curl);
}

static void
full_mode(void)
{
	char *url	= NULL;
	char *dest	= NULL;
	CURL *curl	= NULL;

	url = url_parser(curl, FULL);
	if (!url)
		return;

	/* init curl session */
	curl = curl_easy_init();
	if (!curl)
		goto cleanup;
	
	dest = request_handler(curl, url);
	if (!dest)
		goto cleanup;
	
	/* TODO 
	 * full mode json parser
	 */
	fprintf(stdout, "%s", dest);

cleanup:
	if (dest)
		free(dest);
	if (url)
		free(url);
	if (curl)
		curl_easy_cleanup(curl);
}

static char *
url_parser(CURL *curl, int mode)
{
	char *ret		= NULL;
	char *tmp		= NULL;
	char *curl_escape	= NULL;
	size_t len_ret;

	/* url encoding */
	curl_escape = curl_easy_escape(curl, lang.text, (int)strlen(lang.text));
	char options[SUM_LEN_STRING(curl_escape, lang.src, lang.dest,
			url_params[mode])+1];

	/* formatting string */
	sprintf(options, url_params[mode], lang.src, lang.dest, curl_escape);

	ret = strdup(url_google);
	if (!ret) {
		perror("url_parser(): strdup");
		return NULL;
	}

	tmp = realloc(ret, strlen(ret) + strlen(options) +1);
	if (!tmp) {
		perror("url_parser(): realloc");
		free(ret);
		return NULL;
	}
	ret = tmp;
	strcat(ret, options);
	len_ret = strlen(ret);
	ret[len_ret] = '\0';

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

	/* set url */
	curl_easy_setopt(curl, CURLOPT_URL, url);
	/* set write function helper */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	/* write data to memory */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&mem);
	/* set user-agent */
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	/* set timeout */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); /* timeout 10s */

	/* sending request */
	ccode = curl_easy_perform(curl);
	if (ccode != CURLE_OK) {
		fprintf(stderr, "request_handler(): curl_easy_perform(): %s\n",
				curl_easy_strerror(ccode));
		free(mem.memory);
		return NULL;
	}

	return mem.memory;
}

/* https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html */
static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	char *ptr	= NULL;
	Memory *mem	= (Memory*)data;
	size_t realsize = size * nmemb;
	
	ptr = realloc(mem->memory, mem->size + realsize +1);
	if (!ptr) {
		perror("write_callback()");
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
	/* TODO
	 * using "argparse"
	 */
	const char help[] = "%s SOURCE TARGET [-b] TEXT\n"
				"Example:\n"
				"\t[BRIEF MODE]\n"
				"\t%s en id -b \"hello\"\n"
				"\t[FULL MODE]\n"
				"\t%s en id \"hello\"\n";
	if (argc < 4) {
		fprintf(stderr, help, argv[0], argv[0], argv[0]);
		return 1;
	}

	lang.src = argv[1];
	lang.dest = argv[2];

	if (strcmp(argv[3], "-b") == 0) {
		if (argv[4] == NULL || strlen(argv[4]) == 0) {
			fprintf(stderr, help, argv[0], argv[0], argv[0]);
			return 1;
		}
		lang.text = argv[4];
		brief_mode();
	} else {
		lang.text = argv[3];
		full_mode();
	}

	return 0;
}
