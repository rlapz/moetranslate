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
#define GREEN_BOLD_E	"\033[01;32m"
#define WHITE_BOLD_E	"\033[01;01m"

#define END_E		"\033[00m"

enum {
	ERR = -2,
	NONE,
	BRIEF,
	FULL,
};

typedef struct {
	char *lcode;	/* language code */
	char *lang;
} Language;

/* String and Memory has the same signature/members */
typedef String Memory;

typedef struct {
	int	mode;	/* mode translation */
	char	*src;	/* source language */
	char	*dest;	/* target language */
	char	*text;	/* text/words */
} Translate;

typedef struct {
	char *base_url;
	char *params[2]; /* url parameter (brief and full mode) */
} Url;

/* function declaration */
static int arg_parse(int argc, char **argv);
static void brief_mode(const Translate *tr);
static void full_mode(const Translate *tr);
static char *get_lang(const char *lcode);
static void help(FILE *f);
static String *request_handler(CURL *curl, const String *url);
static String *url_parser(const Translate *tr, CURL *curl);
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *data);

/* config.h for applying patches and the configuration. */
#include "config.h"

/* global vars */
static const char *argv0;

/* function implementations */
static int
arg_parse(int argc, char **argv)
{
	int mode;

	switch (argc) {
	case 2:
		goto opt1;
	case 5:
		goto opt2;
	default:
		goto err;
	}

opt1:
	if (strcmp(argv[1], "-h") == 0) {
		help(stdout);
		return NONE;
	} else {
		goto err;
	}

opt2:
	if (strcmp(argv[1], "-b") == 0)
		mode = BRIEF;
	else if (strcmp(argv[1], "-f") == 0)
		mode = FULL;
	else
		goto err;

	if (get_lang(argv[2]) == NULL) {
		fprintf(stderr, "Unknown \"%s\" language code.\n", argv[2]);
		goto err;
	} else if (get_lang(argv[3]) == NULL ||
			strcmp(argv[3], "auto") == 0) {
		fprintf(stderr, "Unknown \"%s\" language code.\n", argv[3]);
		goto err;
	}

	if (strlen(rtrim(ltrim(argv[4]))) == 0) {
		fputs("Text empty!\n", stderr);
		goto err;
	}

	/* success */
	return mode;

err:
	errno = EINVAL;
	fprintf(stderr, "%s!\n\n", strerror(errno));
	help(stderr);
	return ERR;
}

static void
brief_mode(const Translate *tr)
{
	cJSON *parser, *iterator;
	String *req_str, *url;
	CURL *curl;

	curl = curl_easy_init();
	if (curl == NULL)
		die("brief_mode(): curl_easy_init()");

	url	= url_parser(tr, curl);
	req_str	= request_handler(curl, url);
	/* cJSON parser */
	parser = cJSON_Parse(req_str->value);
	if (parser == NULL) {
		errno = EINVAL;
		die("brief_mode(): cJSON_Parse(): Parsing error!");
	}

#if DEBUG
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" brief_mode()      : content                 :"
			"\n       %s\n\n",
			cJSON_Print(parser));
#endif

	/* dest[i][0][0] */
	cJSON_ArrayForEach(iterator, cJSON_GetArrayItem(parser,0)) {
		cJSON *value = cJSON_GetArrayItem(iterator, 0); /* index: 0 */
		if (cJSON_IsString(value))
			/* send the result to stdout */
			printf("%s", cJSON_GetStringValue(value));
	}
	putchar('\n');

	cJSON_Delete(parser);
	free_string(url);
	free_string(req_str);
	curl_easy_cleanup(curl);
}

static void
full_mode(const Translate *tr)
{
	cJSON *parser, *iterator;
	String *req_str, *url;
	CURL *curl;

	curl = curl_easy_init();
	if (curl == NULL)
		die("full_mode(): curl_easy_init()");

	url	= url_parser(tr, curl);
	req_str	= request_handler(curl, url);

	/* cJSON parser */
	parser = cJSON_Parse(req_str->value);
	if (parser == NULL) {
		errno = EINVAL;
		die("full_mode(): cJSON_Parse(): Parsing error!");
	}

#if DEBUG
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" brief_mode()      : content                 :"
			"\n       %s\n\n",
			cJSON_Print(parser));
#endif


	/* get spelling array item */
	cJSON *spell = cJSON_GetArrayItem(parser->child, cJSON_GetArraySize(parser->child) -1);

	/* source text */
	printf("\"%s\"\n", tr->text);

	/* source spelling */
	cJSON *spell_src = cJSON_GetArrayItem(spell, 3);
	if (cJSON_IsString(spell_src)) {
		printf("( %s )\n", spell_src->valuestring);
	}
	putchar('\n');


	/* get correction */
	char *correct_str;
	cJSON *correct = cJSON_GetArrayItem(parser, 7);
	if (cJSON_IsString(correct->child)) {
		correct_str = correct->child->next->valuestring;
		printf("\n" WHITE_BOLD_E "Did you mean: " END_E
				"\"%s\"" WHITE_BOLD_E " ?" END_E "\n\n",
				correct_str);
	}

	/* dest text */
	cJSON *trans = cJSON_GetArrayItem(parser, 0);
	cJSON *trans_val;
	cJSON_ArrayForEach(iterator, trans) {
		trans_val = cJSON_GetArrayItem(iterator, 0);
		if (cJSON_IsString(trans_val)) {
			printf(WHITE_BOLD_E "%s" END_E, trans_val->valuestring);
		}
	}
	putchar('\n');

	/* dest spelling */
	cJSON *spell_dest = cJSON_GetArrayItem(spell, 2);
	if (cJSON_IsString(spell_dest)) {
		printf("( %s )\n", spell_dest->valuestring);
	}
	putchar('\n');

	/* lang */
	char *lang_dest_str;
	cJSON *lang_dest = cJSON_GetArrayItem(parser, 2);
	if (cJSON_IsString(lang_dest)) {
		lang_dest_str = lang_dest->valuestring;
		printf("\n" WHITE_BOLD_E "[%s]:" END_E " %s", lang_dest_str,
				get_lang(lang_dest_str));
	}

	printf(" -> " WHITE_BOLD_E "[%s]:" END_E " %s\n",
			tr->dest, get_lang(tr->dest));

	/* synonyms */
	cJSON *synonym = cJSON_GetArrayItem(parser, 1);
	cJSON_ArrayForEach(iterator, synonym) {
		cJSON *syn_label = iterator->child;
		syn_label->valuestring[0] = toupper(syn_label->valuestring[0]);

		printf("\n" WHITE_BOLD_E "[%s]:" END_E, syn_label->valuestring);

		cJSON *syn_dest;
		cJSON_ArrayForEach(syn_dest, cJSON_GetArrayItem(iterator, 2)) {
			printf("\n " WHITE_BOLD_E "%s:" END_E "\n\t",
					syn_dest->child->valuestring);

			cJSON *syn_src;
			int syn_src_size = cJSON_GetArraySize(cJSON_GetArrayItem(syn_dest, 1))-1;
			cJSON_ArrayForEach(syn_src, cJSON_GetArrayItem(syn_dest, 1)) {
				printf("%s", syn_src->valuestring);
				if (syn_src_size > 1)
					printf(", ");
				syn_src_size--;
			}
		}
		putchar('\n');
	}

	/* examples */
	char *example_str;
	int max = example_max_line;
	cJSON *example = cJSON_GetArrayItem(parser, 13);

	if (!cJSON_IsNull(example) && cJSON_IsArray(example)) {
		printf("\n\n%s\n", "------------------------");
		cJSON_ArrayForEach(iterator, example) {
			cJSON *example_val;
			cJSON_ArrayForEach(example_val, iterator) {
				if (max == 0)
					break;
				example_str = example_val->child->valuestring;
				example_str[0] = toupper(example_str[0]);
				printf("%s\n", trim_tag(example_str, 'b'));
				max--;
			}
		}
	}

	free_string(url);
	free_string(req_str);
	cJSON_Delete(parser);
	curl_easy_cleanup(curl);
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

static String *
request_handler(CURL *curl, const String *url)
{
	String *ret = calloc(sizeof(String), sizeof(String));
	CURLcode ccode;

	if ((ret->value = malloc(1)) == NULL)
		die("request_handler(): malloc");

	/* set url */
	curl_easy_setopt(curl, CURLOPT_URL, url->value);
	/* set write function helper */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	/* write data to memory */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)ret);
	/* set user-agent */
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
	/* set timeout */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

	/* sending request */
	if ((ccode = curl_easy_perform(curl)) != CURLE_OK)
		die("request_handler(): %s", curl_easy_strerror(ccode));

#if DEBUG
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" request_handler() : request length          : %zu\n",
		        url->length);
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" request_handler() : content length(total)   : %zu\n",
			ret->length);
#endif

	return ret;
}

static String *
url_parser(const Translate *tr, CURL *curl)
{
	String *ret = new_string();
	char *text_encode = curl_easy_escape(curl, tr->text,
				(int)strlen(tr->text));

	if (ret == NULL)
		die("url_parser(): ret");
	if (text_encode == NULL)
		die("url_parser(): curl_easy_escape()");

	int s = append_string(ret, "%s%s&sl=%s&tl=%s&hl=%s&q=%s",
			url_google.base_url, url_google.params[tr->mode],
			tr->src, tr->dest, tr->dest, text_encode);
	if (s == 0)
		die("url_parser(): append_string");

#if DEBUG
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" url_parser()      : request length(total)   : %d\n", s);
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
	char *ptr;
	Memory *mem	= (Memory*)data;
	size_t realsize	= (size * nmemb);

#if DEBUG
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" write_callback()  : content length          : %zu\n", mem->length);
#endif

	ptr = realloc(mem->value, mem->length + realsize +1);
       	if (ptr == NULL)
		die("write_callback(): realloc");

	mem->value = ptr;
	memcpy(&(mem->value[mem->length]), contents, realsize);
	mem->length += realsize;
	mem->value[mem->length] = '\0';

#if DEBUG
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" write_callback()  : content length(real)    : %zu\n", realsize);
	printf(GREEN_BOLD_E "DEBUG:" END_E 
			" write_callback()  : appended content length : %zu\n", mem->length);
#endif

	return realsize;
}

static void
help(FILE *f)
{
	fprintf(f, "moetranslate - Simple language translator\n\n"
			"Usage: %s [-b/-f/-h] [SOURCE] [TARGET] [TEXT]\n"
			"       -b        Brief mode\n"
			"       -f        Full mode\n"
			"       -h        Show this help\n\n"
			"Examples:\n"
			"   Brief Mode:  %s -b en id \"Hello\"\n"
			"   Full Mode :  %s -f id en \"Halo\"\n"
			"   Auto Lang :  %s -f auto en \"こんにちは\"\n",
		argv0, argv0, argv0, argv0);
}


int
main(int argc, char *argv[])
{
#if DEBUG
	printf(GREEN_BOLD_E "[DEBUG Mode]" END_E "\n");
#endif

	argv0 = argv[0];
	int mode = arg_parse(argc, argv);

	if (mode == NONE)
		return EXIT_SUCCESS;
	else if (mode == ERR)
		return EXIT_FAILURE;

	Translate tr;

	tr.mode = mode;
	tr.src  = argv[2];
	tr.dest = argv[3];
	tr.text = argv[4];

	if (tr.mode == BRIEF)
		brief_mode(&tr);
	else
		full_mode(&tr);

	return EXIT_SUCCESS;
}

