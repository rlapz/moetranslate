#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "cJSON.h"


/* macros */
#define SUM_LEN_STRING(A, B, C, D) \
	((strlen(A)) + (strlen(B)) + (strlen(C)) + (strlen(D)))

#define BRIEF	0	/* brief mode */
#define FULL	1	/* full mode */

struct Lang {
	char	*src;	/* source language */
	char	*dest;	/* target language */
	char	*text;	/* text/words */
	int	mode;	/* mode translation */
} __attribute__((__packed__));

struct Memory{
	char	*memory;
	size_t	size;
};

/* function declaration */
static void brief_mode(void);
static void full_mode(void);
static char *url_parser(CURL *curl);
static char *request_handler(void);
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *data);

/* global variables */
static struct Lang lang;
static long timeout		= 10L; /* set timeout request (10s) */
static const char url_google[]	= "https://translate.google.com/translate_a/single?";
static const char *url_params[]	= {
	[BRIEF]	= "client=gtx&ie=UTF-8&oe=UTF-8&sl=%s&tl=%s&dt=t&q=%s",
	[FULL]	= "client=gtx&ie=UTF-8&oe=UTF-8&dt=bd&dt=x&dt=ld&dt=md&dt=rw&"
		  "dt=rm&dt=ss&dt=t&dt=at&dt=gt&dt=qc&sl=%s&tl=%s&hl=id&q=%s"
};

/* function implementations */
static void
brief_mode(void)
{
	char *dest = NULL;
	cJSON *parser, *array, *iterator, *value;

	if (!(dest = request_handler()))
		exit(1);

	/* JSON parser */
	/* dest[i][0][0] */
	if (!(parser = cJSON_Parse(dest))) {
		perror("brief_mode(): cJSON_Parse()");
		goto cleanup;
	}
	if (!(array = cJSON_GetArrayItem(parser, 0))) {
		perror("brief_mode(): cJSON_GetArrayItem()");
		goto cleanup;
	}

	cJSON_ArrayForEach(iterator, array) {
		value = cJSON_GetArrayItem(iterator, 0);
		if (cJSON_IsString(value))
			/* show the result to stdout */
			fprintf(stdout, "%s", cJSON_GetStringValue(value));
	}
	puts("");

cleanup:
	cJSON_Delete(parser);
	free(dest);
}

static void
full_mode(void)
{
	char *dest = NULL;

	/* init curl session */
	if (!(dest = request_handler()))
		exit(1);
	
	/* TODO 
	 * full mode json parser
	 */
	fprintf(stdout, "%s", dest);

	free(dest);
}

static char *
url_parser(CURL *curl)
{
	char *ret		= NULL;
	char *tmp		= NULL;
	char *text_encoding	= NULL;
	size_t length_opt;

	/* text encoding */
	text_encoding = curl_easy_escape(curl, lang.text, (int)strlen(lang.text));
	if (!text_encoding) {
		perror("url_parser(): curl_easy_escape()");
		return NULL;
	}

	length_opt = SUM_LEN_STRING(text_encoding, lang.src, lang.dest,
			url_params[lang.mode]) -5; /* sum(%s%s%s) == 6 chars */

	char options[length_opt];

	/* formatting string */
	snprintf(options, length_opt,
			url_params[lang.mode], lang.src, lang.dest, text_encoding);

	options[length_opt] = '\0';

	if (!(tmp = strndup(url_google, strlen(url_google)))) {
		perror("url_parser(): strndup");
		goto cleanup;
	}

	if (!(ret = realloc(tmp, strlen(tmp) + length_opt +1))) {
		perror("url_parser(): realloc");
		free(tmp);
		goto cleanup;
	}

	/* concat between url_google and options */
	strncat(ret, options, length_opt);

cleanup:
	curl_free(text_encoding);
	if (ret)
		return ret;
	return NULL;
}

static char *
request_handler(void)
{
	char *url		= NULL;
	CURL *curl		= NULL;
	struct Memory mem	= {NULL, 0};
	CURLcode ccode;

	/* curl init */
	if (!(curl = curl_easy_init())) {
		perror("request_handler(): curl_easy_init()");
		return NULL;
	}
	/* url parser */
	if (!(url = url_parser(curl))) {
		perror("request_handler(): url_parser()");
		goto cleanup;
	}
	if (!(mem.memory = malloc(1))) {
		perror("request_handler(): malloc()=>Memory.memory");
		goto cleanup;
	}

	/* set url */
	curl_easy_setopt(curl, CURLOPT_URL, url);
	/* set write function helper */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	/* write data to memory */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&mem);
	/* set user-agent */
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	/* set timeout */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

	/* sending request */
	if ((ccode = curl_easy_perform(curl)) != CURLE_OK) {
		fprintf(stderr, "request_handler(): curl_easy_perform(): %s\n",
				curl_easy_strerror(ccode));
		free(mem.memory);
		mem.memory = NULL;
		goto cleanup;
	}

cleanup:
	curl_easy_cleanup(curl);
	if (url)
		free(url);
	if (mem.memory)
		return mem.memory;
	return NULL;
}

/* https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html */
static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	char *ptr		= NULL;
	struct Memory *mem	= (struct Memory*)data;
	size_t realsize		= (size * nmemb);
	
	if (!(ptr = realloc(mem->memory, mem->size + realsize +1))) {
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
			return EXIT_FAILURE;
		}
		lang.text = argv[4];
		lang.mode = BRIEF;
		brief_mode();
	} else {
		lang.text = argv[3];
		lang.mode = FULL;
		full_mode();
	}

	return EXIT_SUCCESS;
}
