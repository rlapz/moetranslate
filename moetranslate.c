/* MIT License
 *
 * Copyright (c) 2021 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "cJSON.h"
#include "util.h"

/* macros */
#define GREEN_E		"\033[00;32m"
#define YELLOW_E	"\033[00;33m"

#define GREEN_BOLD_E	"\033[01;32m"
#define YELLOW_BOLD_E	"\033[01;33m"
#define BLUE_BOLD_E	"\033[01;34m"
#define WHITE_BOLD_E	"\033[01;37m"

#define END_E		"\033[00m"

enum {
	BRIEF,
	FULL,
	DETECT
};

typedef struct {
	char *lcode;	/* language code */
	char *lang;
} Language;

typedef struct {
	int  mode;	 /* mode translation */
	char *src;	 /* source language */
	char *target;    /* target language */
	char *text;	 /* text/words */
} Translate;

typedef struct {
	char *base_url;
	char *params[3]; /* url parameter (brief, full mode, detect lang) */
} Url;

/* function declaration */
static void   brief_mode(const cJSON *result);
static void   detect_lang(const cJSON *result);
static void   full_mode(const cJSON *result);
static char   *get_lang(const char *lcode);
static void   get_result(void);
static void   help(FILE *out);
static char   *request_handler(CURL *curl, const String *url);
static String *url_parser(CURL *curl);
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *data);

/* config.h for applying patches and the configuration. */
#include "config.h"

/* global vars */
static const Translate *tr;

/* function implementations */
static void
brief_mode(const cJSON *result)
{
	cJSON *i, *value;

	/* result[i][0][0] */
	cJSON_ArrayForEach(i, result->child) {
		value = i->child; /* index: 0 */
		if (cJSON_IsString(value))
			/* send the result to stdout */
			printf("%s", value->valuestring);
	}
	putchar('\n');
}

static void
detect_lang(const cJSON *result)
{
	cJSON *lang_src = cJSON_GetArrayItem(result, 2);

	if (cJSON_IsString(lang_src)) {
		char *l = lang_src->valuestring;
		printf("%s (%s)\n", l, get_lang(l));
	}
}

static void
full_mode(const cJSON *result)
{
	cJSON *i; /* iterator  */

	/* source text */
	printf("\"%s\"\n", tr->text);


	/* get correction */
	cJSON *correction = cJSON_GetArrayItem(result, 7);

	if (cJSON_IsString(correction->child) &&
			cJSON_IsString(correction->child->next)) {
		printf("\n" YELLOW_BOLD_E "Did you mean: " END_E
				"\"%s\"" YELLOW_BOLD_E " ?\n\n" END_E,
				correction->child->next->valuestring);
	}


	/* get spelling array item */
	cJSON *spelling = cJSON_GetArrayItem(result->child, 
				cJSON_GetArraySize(result->child) -1);


	/* source spelling */
	cJSON *spelling_src = cJSON_GetArrayItem(spelling, 3);

	if (cJSON_IsString(spelling_src)) {
		printf("( " YELLOW_E "%s" END_E " )\n", spelling_src->valuestring);
	}


	/* source lang */
	cJSON *lang_src = cJSON_GetArrayItem(result, 2);

	if (cJSON_IsString(lang_src)) {
		char *lang_src_str = lang_src->valuestring;
		printf(GREEN_E "[ %s ]:" END_E " %s\n\n", lang_src_str,
				get_lang(lang_src_str));
	}


	/* target text */
	cJSON *trans = result->child;
	cJSON *trans_val;

	cJSON_ArrayForEach(i, trans) {
		trans_val = i->child;
		if (cJSON_IsString(trans_val)) {
			printf(WHITE_BOLD_E "%s" END_E, trans_val->valuestring);
		}
	}
	putchar('\n');


	/* target spelling */
	cJSON *spelling_target = cJSON_GetArrayItem(spelling, 2);

	if (cJSON_IsString(spelling_target)) {
		printf("( " YELLOW_E "%s" END_E " )\n", spelling_target->valuestring);
	}


	/* target lang*/
	printf( GREEN_E "[ %s ]:" END_E " %s\n",
			tr->target, get_lang(tr->target));


	/* synonyms */
	cJSON *synonym = cJSON_GetArrayItem(result, 1);
	cJSON *syn_label,
	      *syn_src,
	      *syn_target;

	if (cJSON_IsArray(synonym))
		printf("\n%s", "~~~~~~~~~~~~~~~~~~~~~~~~");

	cJSON_ArrayForEach(i, synonym) {
		int max_syn = synonym_max_line;
		syn_label   = i->child; /* Verb, Noun, etc */

		printf("\n" BLUE_BOLD_E "[ %s ]" END_E, syn_label->valuestring);

		/* target words alternatives */
		cJSON_ArrayForEach(syn_target, cJSON_GetArrayItem(i, 2)) {
			if (max_syn == 0)
				break;
			if (max_syn > 0)
				max_syn--;

			printf("\n  " WHITE_BOLD_E "%s:" END_E "\n\t"
					YELLOW_E "-> " END_E,
					syn_target->child->valuestring);

			/* source word alternatives */
			int syn_src_size = cJSON_GetArraySize(cJSON_GetArrayItem(
						syn_target, 1))-1;
			cJSON_ArrayForEach(syn_src, cJSON_GetArrayItem(syn_target, 1)) {
				printf("%s", syn_src->valuestring);

				if (syn_src_size > 0) {
					printf(", ");
					syn_src_size--;
				}
			}
		}
		putchar('\n');
	}


	/* examples */
	cJSON *example = cJSON_GetArrayItem(result, 12);
	cJSON *example_sub,
	      *example_desc,
	      *example_val,
	      *example_exp;

	if (cJSON_IsArray(example))
		printf("\n\n%s", "~~~~~~~~~~~~~~~~~~~~~~~~");

	cJSON_ArrayForEach(i, example) {
		cJSON *example_label = i->child;

		if (strlen(example_label->valuestring) > 0)
			printf("\n" YELLOW_BOLD_E "[ %s ]" END_E,
					example_label->valuestring);

		cJSON_ArrayForEach(example_sub, cJSON_GetArrayItem(i, 1)) {
			example_desc = example_sub->child;
			example_val  = cJSON_GetArrayItem(example_sub, 2);
			example_exp  = cJSON_GetArrayItem(example_sub, 3);

			printf("\n  " WHITE_BOLD_E "%s" END_E "\n\t",
					example_desc->valuestring);

			if (cJSON_IsString(example_val))
				printf(YELLOW_E "->" END_E
					       	" %s ", example_val->valuestring);

			if (cJSON_IsArray(example_exp) &&
					cJSON_IsString(example_exp->child->child)) {
				printf(GREEN_E "[ %s ]" END_E
					, example_exp->child->child->valuestring);
			}
		}
		putchar('\n');
	}

	/* more examples */
	cJSON *more_example = cJSON_GetArrayItem(result, 13);
	cJSON *more_example_val;

	if (cJSON_IsArray(more_example)) {
		printf("\n\n%s\n", "~~~~~~~~~~~~~~~~~~~~~~~~");

		/* because *result has const attribute */
		String *example_str = new_string();
		int    example_max  = example_max_line;

		cJSON_ArrayForEach(i, more_example) {
			cJSON_ArrayForEach(more_example_val, i) {
				if (example_max == 0)
					break;
				if (example_max > 0)
					example_max--;

				append_string(example_str,
						"\"" YELLOW_E "%s" END_E "\"\n", 
						more_example_val->child->valuestring);
			}
		}
		/* eliminating <b> ... </b> tags */
		trim_tag(example_str, 'b');
		printf("%s\n", example_str->value);

		free_string(example_str);
	}
}

static char *
get_lang(const char *lcode)
{
	size_t lang_len = LENGTH(language);
	for (size_t i = 0; i < lang_len; i++) {
		if (strncmp(lcode, language[i].lcode, 5) == 0)
			return (char*)language[i].lang;
	}

	return NULL;
}

static void
get_result(void)
{
	char   *req_str;
	cJSON  *result;
	String *url;
	CURL   *curl;

	curl = curl_easy_init();
	if (curl == NULL)
		die("get_result(): curl_easy_init()");

	url	= url_parser(curl);
	req_str	= request_handler(curl, url);
	result	= cJSON_Parse(req_str);

#if DEBUG 
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" get_result()      : content                 :"
			"\n       %s\n\n",
			cJSON_Print(result));
#endif

	if (result == NULL) {
		errno = EINVAL;
		die("get_result(): cJSON_Parse(): Parsing error!");
	}

	switch (tr->mode) {
	case BRIEF:
		brief_mode(result);
		break;
	case FULL:
		full_mode(result);
		break;
	case DETECT:
		detect_lang(result);
		break;
	}

	free(req_str);
	free_string(url);
	curl_easy_cleanup(curl);
	cJSON_Delete(result);
}

static char *
request_handler(CURL *curl, const String *url)
{
	CURLcode ccode;
	String   ret = {NULL, 0};

	if ((ret.value = malloc(1)) == NULL)
		die("request_handler(): malloc");

	/* set url */
	curl_easy_setopt(curl, CURLOPT_URL,           url->value    );
	/* set write function helper */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	/* write data to memory */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA,     (void*)&ret   );
	/* set user-agent */
	curl_easy_setopt(curl, CURLOPT_USERAGENT,     user_agent    );
	/* set timeout */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT,       timeout       );

	/* sending request */
	if ((ccode = curl_easy_perform(curl)) != CURLE_OK)
		die("request_handler(): %s", curl_easy_strerror(ccode));

#if DEBUG
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" request_handler() : request length          : %zu\n",
		        url->length);
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" request_handler() : content length(total)   : %zu\n",
			ret.length);
#endif

	return ret.value;
}

static String *
url_parser(CURL *curl)
{
	char   *text_encode;
	String *ret;

	if (tr == NULL)
		die("url_parser(): tr");

	ret = new_string();
	if (ret == NULL)
		die("url_parser(): ret");

	text_encode = curl_easy_escape(curl, tr->text, (int)strlen(tr->text));
	if (text_encode == NULL)
		die("url_parser(): curl_easy_escape()");

	int ret_size = 0;
	if (tr->mode == DETECT)
		ret_size = append_string(ret, "%s%s&q=%s",
				url_google.base_url, url_google.params[DETECT],
				text_encode);
	else
		ret_size = append_string(ret, "%s%s&sl=%s&tl=%s&hl=%s&q=%s",
				url_google.base_url, url_google.params[tr->mode],
				tr->src, tr->target, tr->target, text_encode);

	if (ret_size == 0)
		die("url_parser(): append_string");

#if DEBUG
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" url_parser()      : request length(total)   : %d\n", ret_size);
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" url_parser()      : request url             :\n       %s\n\n",
			ret->value);
#endif

	curl_free(text_encode);

	return ret;
}

/* https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html */
static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	char   *ptr;
	String *str	= (String*)data;
	size_t realsize = (size * nmemb);

#if DEBUG
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" write_callback()  : content length          : %zu\n", str->length);
#endif

	ptr = realloc(str->value, str->length + realsize +1);
       	if (ptr == NULL)
		die("write_callback(): realloc");

	memcpy(ptr + str->length, contents, realsize);

	str->value               = ptr;
	str->length             += realsize;
	str->value[str->length]  = '\0';

#if DEBUG
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" write_callback()  : content length(real)    : %zu\n", realsize);
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" write_callback()  : appended content length : %zu\n", str->length);
#endif

	return realsize;
}

static void
help(FILE *out)
{
	if (out == stderr) {
		errno = EINVAL;
		perror(NULL);
	}

	fprintf(out, "moetranslate - A simple language translator\n\n"
			"Usage: moetranslate [-b/-f/-d/-h] [SOURCE] [TARGET] [TEXT]\n"
			"       -b         Brief mode\n"
			"       -f         Full mode\n"
			"       -d         Detect language\n"
			"       -h         Show this help\n\n"
			"Examples:\n"
			"   Brief Mode  :  moetranslate -b en:id \"Hello\"\n"
			"   Full Mode   :  moetranslate -f id:en \"Halo\"\n"
			"   Auto Lang   :  moetranslate -f auto:en \"こんにちは\"\n"
			"   Detect Lang :  moetranslate -d \"你好\"\n"
	       );

}


int
main(int argc, char *argv[])
{
#if DEBUG
	printf(GREEN_BOLD_E "[DEBUG Mode]" END_E "\n");
#endif

	/* dumb arg parser */
	if (argc == 2 && strcmp(argv[1], "-h") == 0) {
		help(stdout);
		return EXIT_SUCCESS;
	}

	Translate t = {0};
	tr = &t;

	if (argc == 3 && strcmp(argv[1], "-d") == 0) {
		t.mode = DETECT;
		t.text = argv[2];
		get_result();
		return EXIT_SUCCESS;
	}

	if (argc != 4)
		goto err;

	char *src    = strtok(argv[2], ":"),
	     *target = strtok(NULL,    ":");

	if (src == NULL || target == NULL)
		goto err;
	if (get_lang(src) == NULL) {
		fprintf(stderr, "Unknown \"%s\" language code\n", src);
		goto err;
	}
	if (get_lang(target) == NULL || strcmp(target, "auto") == 0) {
		fprintf(stderr, "Unknown \"%s\" language code\n", target);
		goto err;
	}
	if (strcmp(argv[1], "-b") == 0)
		t.mode = BRIEF;
	else if (strcmp(argv[1], "-f") == 0)
		t.mode = FULL;
	else
		goto err;


	t.src    = src;
	t.target = target;
	t.text	 = rtrim(ltrim(argv[3]));
	get_result();

	return EXIT_SUCCESS;

err:
	help(stderr);
	return EXIT_FAILURE;
}

