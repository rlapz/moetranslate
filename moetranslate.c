/* MIT License
 *
 * Copyright (c) 2021 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */

#include <ctype.h>
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
	char	*lcode;	   /* language code    */
	char	*lang;
} Language;

typedef struct {
	char	*memory;
	size_t	 size;
} Memory;

typedef struct {
	int	 mode;	   /* mode translation */
	char   	*src;	   /* source language  */
	char   	*target;   /* target language  */
	char   	*text;	   /* text/words       */
} Translate;

typedef struct {
	char	*base_url;
	char   	*params[3]; /* url parameter (brief, full mode, detect lang) */
} Url;

/* function declaration, ordered in logical manner */
static const char *get_lang        (const char *lcode);
static void        get_result      (const Translate *tr);
static char       *url_parser      (char *dest, size_t len, const Translate *tr);
static void        request_handler (Memory *dest, CURL *curl, const char *url);
static size_t      write_callback  (char *ptr, size_t size, size_t nmemb, void *data);
static void        brief_mode      (const cJSON *result);
static void        detect_lang     (const cJSON *result);
static void        full_mode       (const Translate *tr, cJSON *result);
static void        help            (FILE *out);

/* config.h for applying patches and the configuration. */
#include "config.h"


/* function implementations */
static const char *
get_lang(const char *lcode)
{
	size_t lang_len = LENGTH(language);
	for (size_t i = 0; i < lang_len; i++) {
		if (strncmp(lcode, language[i].lcode, 5) == 0)
			return language[i].lang;
	}

	return NULL;
}

static void
get_result(const Translate *tr)
{
	char	 url[(TEXT_MAX_LEN * 3) + 150];
	CURL	*curl;
	cJSON	*result;
	Memory	 mem = {NULL, 0};

	curl = curl_easy_init();
	if (curl == NULL)
		die("get_result(): curl_easy_init()");

	url_parser(url, sizeof(url), tr);
	request_handler(&mem, curl, url);

	result = cJSON_Parse(mem.memory);
	if (result == NULL) {
		errno = EINVAL;
		die("get_result(): cJSON_Parse(): Parsing error!");
	}

	switch (tr->mode) {
	case BRIEF:
		brief_mode(result);
		break;
	case FULL:
		full_mode(tr, result);
		break;
	case DETECT:
		detect_lang(result);
		break;
	}

	free(mem.memory);
	cJSON_Delete(result);
	curl_easy_cleanup(curl);
}

static char *
url_parser(char *dest, size_t len, const Translate *tr)
{
	int  ret;
	char text_encode[TEXT_MAX_LEN * 3];

	url_encode(text_encode, (unsigned char *)tr->text, sizeof(text_encode));

	ret = snprintf(dest, len, "%s%s",
			url_google.base_url, url_google.params[tr->mode]);

	if (ret < 0)
		die("url_parser(): formatting url");

	switch (tr->mode) {
	case DETECT:
		ret = snprintf(dest + ret, len, "&q=%s", text_encode);
		break;
	case BRIEF:
		ret = snprintf(dest + ret, len, "&sl=%s&tl=%s&q=%s",
				tr->src, tr->target, text_encode);
		break;
	case FULL:
		ret = snprintf(dest + ret, len, "&sl=%s&tl=%s&hl=%s&q=%s",
				tr->src, tr->target, tr->target, text_encode);
		break;
	default:
		die("url_parser(): mode is invalid");
	}

	if (ret < 0)
		die("url_parser(): formatting url");

	return dest;
}

static void
request_handler(Memory *dest, CURL *curl, const char *url)
{
	CURLcode ccode;

	curl_easy_setopt(curl, CURLOPT_URL,		url		);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,	write_callback	);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA,	(void*)dest	);
	curl_easy_setopt(curl, CURLOPT_USERAGENT,	user_agent	);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT,		timeout		);

	ccode = curl_easy_perform(curl);
	if (ccode != CURLE_OK)
		die("request_handler(): %s", curl_easy_strerror(ccode));
}

/* https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html */
static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	char   *ptr;
	Memory *mem	 = (Memory *)data;
	size_t	realsize = (size * nmemb);

	ptr = realloc(mem->memory, mem->size + realsize +1);
	if (ptr == NULL)
		die("write_callback(): realloc");

	memcpy(ptr + mem->size, contents, realsize);

	mem->memory		 = ptr;
	mem->size		+= realsize;
	mem->memory[mem->size]	 = '\0';

	return realsize;
}

static void
brief_mode(const cJSON *result)
{
	cJSON *i, *value;

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
	/* faster than cJSON_GetArrayItem(result, 2)
	 * (without iterations) but UNSAFE */ 
	cJSON *lang_src = result->child->next->next;

	if (cJSON_IsString(lang_src)) {
		char *l = lang_src->valuestring;
		printf("%s (%s)\n", l, get_lang(l));
	}
}

static void
full_mode(const Translate *tr, cJSON *result)
{
	/*
	   source text 
	  	|
	   correction
	  	|
	 source spelling
	  	|
	   source lang
	  	|
	   target text
	  	|
	  target speling
	  	|
	   target lang
	  	|
	    synonyms
	  	|
	   definitions
	  	|
	    examples
	 */

	cJSON *i; /* iterator  */
	cJSON *trans_text	= result->child;

	cJSON *examples		= cJSON_GetArrayItem(result, 13);
	cJSON *definitions	= cJSON_GetArrayItem(result, 12);
	cJSON *spelling		= cJSON_GetArrayItem(trans_text,
					cJSON_GetArraySize(trans_text) -1);
	cJSON *src_correction	= cJSON_GetArrayItem(result, 7);
	cJSON *src_lang		= cJSON_GetArrayItem(result, 2);
	cJSON *synonyms		= cJSON_GetArrayItem(result, 1);

	cJSON *src_spelling	= cJSON_GetArrayItem(spelling, 3);
	cJSON *tgt_spelling	= cJSON_GetArrayItem(spelling, 2);


	cJSON *src_syn, *tgt_syn;		/* synonyms	*/
	cJSON *def_sub, *def_val, *def_oth;	/* definitions	*/
	cJSON *expl_val;			/* examples	*/


	/* source text */
	printf("\"%s\"\n", tr->text);

	/* correction */
	if (cJSON_IsString(src_correction->child)) {
		printf("\n" YELLOW_BOLD_E "Did you mean: " END_E
			"\"%s\"" YELLOW_BOLD_E " ?\n\n" END_E,
			src_correction->child->next->valuestring);
	}

	/* source spelling */
	if (cJSON_IsString(src_spelling)) {
		printf("( " YELLOW_E "%s" END_E " )\n",
				src_spelling->valuestring);
	}


	/* source lang */
	if (cJSON_IsString(src_lang)) {
		printf(GREEN_E "[ %s ]:" END_E " %s\n\n",
				src_lang->valuestring,
				get_lang(src_lang->valuestring));
	}

	/* target text */
	cJSON_ArrayForEach(i, trans_text) {
		if (cJSON_IsString(i->child))
			printf(WHITE_BOLD_E "%s" END_E, i->child->valuestring);
	}

	putchar('\n');

	/* target spelling */
	if (cJSON_IsString(tgt_spelling)) {
		printf("( " YELLOW_E "%s" END_E " )\n",
				tgt_spelling->valuestring);
	}

	/* target lang */
	printf( GREEN_E "[ %s ]:" END_E " %s\n",
			tr->target, get_lang(tr->target));

	putchar('\n');

	/* synonyms */
	if (!cJSON_IsArray(synonyms) || synonym_max_line == 0)
		goto l_definitions;

	printf("\n%s", "------------------------");

	char *syn_lbl_str; /* label */
	cJSON_ArrayForEach(i, synonyms) {
		int   iter	= 1;
		int   syn_max	= synonym_max_line;
		char *tgt_syn_str;

		syn_lbl_str = i->child->valuestring;	  /* Verb, Noun, etc */
		syn_lbl_str[0] = toupper(syn_lbl_str[0]);

		printf("\n" BLUE_BOLD_E "[ %s ]" END_E, syn_lbl_str);

		/* target alternatives */
		cJSON_ArrayForEach(tgt_syn, cJSON_GetArrayItem(i, 2)) {
			if (syn_max == 0)
				break;

			tgt_syn_str = tgt_syn->child->valuestring;
			tgt_syn_str[0] = toupper(tgt_syn_str[0]);

			printf("\n" WHITE_BOLD_E "%d. %s:" END_E "\n\t"
					YELLOW_E "-> " END_E,
					iter, tgt_syn_str);

			/* source alternatives */
			int syn_src_size = cJSON_GetArraySize(cJSON_GetArrayItem(
						tgt_syn, 1)) -1;
			cJSON_ArrayForEach(src_syn,
					cJSON_GetArrayItem(tgt_syn, 1)) {
				printf("%s", src_syn->valuestring);

				if (syn_src_size > 0) {
					printf(", ");
					syn_src_size--;
				}
			}
			iter++;
			syn_max--;
		}
		putchar('\n');
	}
	putchar('\n');


l_definitions:
	/* definitions */
	if (!cJSON_IsArray(definitions) || definition_max_line == 0)
		goto l_example;

	printf("\n%s", "------------------------");

	char *def_lbl_str; /* label */
	cJSON_ArrayForEach(i, definitions) {
		int   iter	= 1;
		int   def_max	= definition_max_line;
		char *def_sub_str;

		def_lbl_str = i->child->valuestring;

		if (strlen(def_lbl_str) == 0)
			continue;

		def_lbl_str[0] = toupper(def_lbl_str[0]);
		printf("\n" YELLOW_BOLD_E "[ %s ]" END_E, def_lbl_str);

		cJSON_ArrayForEach(def_sub, cJSON_GetArrayItem(i, 1)) {
			if (def_max == 0)
				break;

			def_val	= cJSON_GetArrayItem(def_sub, 2);
			def_oth	= cJSON_GetArrayItem(def_sub, 3);

			def_sub_str	= def_sub->child->valuestring;
			def_sub_str[0]	= toupper(def_sub_str[0]);

			printf("\n" WHITE_BOLD_E "%d. %s" END_E "\n\t",
					iter, def_sub_str);

			if (cJSON_IsString(def_val)) {
				printf(YELLOW_E "->" END_E " %s ",
						def_val->valuestring);
			}

			if (cJSON_IsArray(def_oth) &&
					cJSON_IsString(def_oth->child->child)) {
				printf(GREEN_E "[ %s ]" END_E,
					def_oth->child->child->valuestring);
			}
			iter++;
			def_max--;
		}
		putchar('\n');
	}
	putchar('\n');

l_example:
	if (!cJSON_IsArray(examples) || example_max_line == 0)
		return; /* it's over */

	printf("\n%s\n", "------------------------");

	int  iter	= 1;
	int  expl_max	= example_max_line;
	char *expl_str;
	cJSON_ArrayForEach(i, examples) {
		cJSON_ArrayForEach(expl_val, i) {
			if (expl_max == 0)
				break;

			expl_str = expl_val->child->valuestring;

			/* eliminating <b> ... </b> tags */
			trim_tag(expl_str, 'b');

			expl_str[0] = toupper(expl_str[0]);

			printf("%d. " YELLOW_E "%s" END_E "\n",
					iter, expl_str);

			iter++;
			expl_max--;
		}
		putchar('\n');
	}
}

static void
help(FILE *out)
{
	if (out == stderr) {
		errno = EINVAL;
		perror(NULL);
	}

	fprintf(out,
		"moetranslate - A simple language translator\n\n"
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
	/* dumb arg parser */
	if (argc == 2 && strcmp(argv[1], "-h") == 0) {
		help(stdout);
		return EXIT_SUCCESS;
	}

	Translate t = {0, NULL, NULL, NULL};

	if (argc == 3 && strcmp(argv[1], "-d") == 0) {
		t.mode = DETECT;
		t.text = rtrim(ltrim(argv[2]));
		goto result;
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

	if (strcmp(target, "auto") == 0 || get_lang(target) == NULL) {
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
	t.text	 = ltrim(rtrim(argv[3]));

result:
	if (strlen(t.text) >= TEXT_MAX_LEN) {
		fprintf(stderr, "Text too long, MAX length: %d characters\n",
				TEXT_MAX_LEN);
		goto err;
	}

	get_result(&t);
	return EXIT_SUCCESS;

err:
	help(stderr);
	return EXIT_FAILURE;
}

