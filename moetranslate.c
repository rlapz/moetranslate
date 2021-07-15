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

#include "util.h"
#include "cJSON.h"

/* macros */
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

typedef struct {
	char	*memory;
	size_t	size;
} Memory;

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
static void brief_mode(Translate *tr);
static void full_mode(Translate *tr);
static char *get_lang(const char *lcode);
static char *request_handler(CURL *curl, const char *url);
static char *url_parser(Translate *tr, CURL *curl);
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *data);

static void help(FILE *f);

/* config.h for applying patches and the configuration. */
#include "config.h"

static const char *argv0;

/* function implementations */
static int
arg_parse(int argc, char **argv)
{
	int mode = 0;

	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0) {
			help(stdout);
			return NONE;
		}
	} else if (argc == 5) {
		if (strcmp(argv[1], "-b") == 0)
			mode = BRIEF;
		else if (strcmp(argv[1], "-f") == 0)
			mode = FULL;
	} else {
		goto err;
	}

	if (get_lang(argv[2]) == NULL) {
		fprintf(stderr, "Unknown \"%s\" language code.\n", argv[2]);
		goto err;
	} else if (get_lang(argv[3]) == NULL) {
		fprintf(stderr, "Unknown \"%s\" language code.\n", argv[3]);
		goto err;
	}

	return mode;

err:
	errno = EINVAL;
	fprintf(stderr, "%s!\n\n", strerror(errno));
	help(stderr);
	return ERR;
}

static void
brief_mode(Translate *tr)
{
	cJSON *parser, *iterator;
	char *req_str, *url;
	CURL *curl = NULL;

	if ((curl = curl_easy_init()) == NULL)
		die("brief_mode(): curl_easy_init()");

	url	= url_parser(tr, curl);
	req_str	= request_handler(curl, url);

	/* cJSON parser */
	if ((parser = cJSON_Parse(req_str)) == NULL) {
		errno = EINVAL;
		die("brief_mode(): cJSON_Parse()");
	}

	/* dest[i][0][0] */
	cJSON_ArrayForEach(iterator, cJSON_GetArrayItem(parser,0)) {
		cJSON *value = cJSON_GetArrayItem(iterator, 0); /* index: 0 */
		if (cJSON_IsString(value))
			/* send the result to stdout */
			printf("%s", cJSON_GetStringValue(value));
	}
	putchar('\n');

	cJSON_Delete(parser);
	free(url);
	free(req_str);
	curl_easy_cleanup(curl);
}

static void
full_mode(Translate *tr)
{
	cJSON *parser, *iterator;
	char *req_str, *url;
	CURL *curl = NULL;

	if ((curl = curl_easy_init()) == NULL)
		die("full_mode(): curl_easy_init()");

	url	= url_parser(tr, curl);
	req_str	= request_handler(curl, url);
	
	/* test  */
	// fprintf(stdout, "%s\n", req_str);

	/* cJSON parser */
	if ((parser = cJSON_Parse(req_str)) == NULL) {
		errno = EINVAL;
		die("full_mode(): cJSON_Parse(): Parsing error!");
	}

	/* cJSON Parser */
	/* get translation */
	char *trans_src	= STRING_NEW();
	char *trans_dest = STRING_NEW();
	int count_tr	= 0;
	cJSON *trans	= cJSON_GetArrayItem(parser, 0);

	if (trans_src == NULL || trans_src == NULL)
		die("full_mode()");

	cJSON_ArrayForEach(iterator, trans) {
		cJSON *trans_val = cJSON_GetArrayItem(iterator, 0);
		if (cJSON_IsString(trans_val)) {
			string_append(&trans_dest, "\033[1m\033[37m%s\033[0m",
					trans_val->valuestring);
			string_append(&trans_src, "%s",
					trans_val->next->valuestring);
		}
		count_tr++;
	}
	if (strlen(trans_src) == 0)
		die("full_mode(): Result empty!");


	/* get spelling */
	char *spell_str	= STRING_NEW();
	int count_spell	= 0;
	cJSON *spell	= cJSON_GetArrayItem(cJSON_GetArrayItem(parser, 0),
							count_tr -1);
	if (spell_str == NULL)
		die("full_mode()");

	if (cJSON_GetArraySize(spell) < 6) {
		string_append(&spell_str, "\n");
		cJSON_ArrayForEach(iterator, spell) {
			if (cJSON_IsNull(iterator) ||
					cJSON_IsNumber(iterator) ||
					cJSON_IsArray(iterator)) {
				continue;
			}
			string_append(&spell_str, "( %s )",
						iterator->valuestring);
			count_spell++;
		}
		if (count_spell > 0)
			string_append(&spell_str, "\n");
	}

	/* get correction */
	char *correct_str	= STRING_NEW();
	cJSON *correct		= cJSON_GetArrayItem(parser, 7);
	
	if (correct_str == NULL)
		die("full_mode()");

	if (cJSON_IsString(correct->child)) {
		free(trans_src);
		trans_src = STRING_NEW();
		if (trans_src == NULL)
			die("full_mode()");

		string_append(&correct_str,
				"\n\033[1m\033[37mDid you mean: \033[0m\"%s\"?\n",
				correct->child->next->valuestring);
		string_append(&trans_src, correct->child->next->valuestring);
	}

	/* get language */
	char *lang_v;
	char *lang_str		= STRING_NEW();
	cJSON *langdest		= cJSON_GetArrayItem(parser, 2);

	if (lang_str == NULL)
		die("full_mode()");

	if (cJSON_IsString(langdest)) {
		lang_v = get_lang(langdest->valuestring);
		string_append(&lang_str, "\n[%s]: %s",
				langdest->valuestring,
				lang_v ? lang_v : "");
	}

	/* get synonyms */
	char *syn_tmp;
	char *syn_str	= STRING_NEW();
	int count_syn	= 0;
	cJSON *synonym	= cJSON_GetArrayItem(parser, 1);

	if (syn_str == NULL)
		die("full_mode()");

	cJSON_ArrayForEach(iterator, synonym) {
		syn_tmp = iterator->child->valuestring;
		syn_tmp[0] = TOUPPER(syn_tmp[0]);

		if (count_syn > 0)
			string_append(&syn_str, "\n");

		string_append(&syn_str, "\n\033[1m\033[37m[%s]:\033[0m",
				syn_tmp);

		cJSON *syn_val1;
		cJSON_ArrayForEach(syn_val1, cJSON_GetArrayItem(iterator, 2)) {
			string_append(&syn_str, "\n  \033[1m\033[37m%s:\033[0m\n\t",
					syn_val1->child->valuestring);

			cJSON *syn_val2;
			cJSON_ArrayForEach(syn_val2,
					cJSON_GetArrayItem(syn_val1, 1)) {
				string_append(&syn_str, "%s, ",
						syn_val2->valuestring);

			}
			syn_str[strlen(syn_str)-2] = '.';
		}
		count_syn++;
	}
	if (count_syn > 0)
		string_append(&syn_str, "\n");

	/* get examples */
	char *example_str	= STRING_NEW();
	int max			= example_max_line;
	cJSON *example		= cJSON_GetArrayItem(parser, 13);

	if (example_str == NULL)
		die("full_mode()");

	if (!cJSON_IsNull(example)) {
		string_append(&example_str, "\n%s\n",
				"------------------------------------");
		cJSON_ArrayForEach(iterator, example ) {
			cJSON *example_val;
			cJSON_ArrayForEach(example_val, iterator) {
				if (max == 0)
					break;
				string_append(&example_str, "\"%s\"\n",
						example_val->child->valuestring);
				max--;
			}
		}
		trim_tag(&example_str, 'b');
	}
	
	/* output */
	/* print to stdout */
	fprintf(stdout, "%s\"%s\"%s\n\n%s\n[%s]: %s\n%s%s%s",
			correct_str,
			trans_src, lang_str, trans_dest,
			tr->dest, get_lang(tr->dest),
			spell_str, syn_str, example_str);

#if DEBUG
	printf("\n-------\nSrc len: %zu\n", strlen(trans_src));
	printf("Dest len: %zu\n", strlen(trans_dest));
#endif

	free(trans_src);
	free(trans_dest);
	free(spell_str);
	free(lang_str);
	free(syn_str);
	free(correct_str);
	free(example_str);

	free(url);
	free(req_str);
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

static char *
request_handler(CURL *curl, const char *url)
{
	Memory mem	= { NULL, 0 };
	CURLcode ccode;

	if ((mem.memory = malloc(1)) == NULL)
		die("request_handler(): malloc()=>Memory.memory");

	/* set url */
	curl_easy_setopt(curl, CURLOPT_URL, url);
	/* set write function helper */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	/* write data to memory */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&mem);
	/* set user-agent */
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
	/* set timeout */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

	/* sending request */
	if ((ccode = curl_easy_perform(curl)) != CURLE_OK)
		die("%s: %s", "request_handler()", curl_easy_strerror(ccode));

	return mem.memory;
}

static char *
url_parser(Translate *tr, CURL *curl)
{
	char *ret = STRING_NEW();
	if (ret == NULL)
		die("url_parser()");

	char *text_encode = curl_easy_escape(curl, tr->text,
				(int)strlen(tr->text));
	if (text_encode == NULL)
		die("url_parser(): curl_easy_escape()");

	string_append(&ret, "%s%s&sl=%s&tl=%s&q=%s",
			url_google.base_url, url_google.params[tr->mode],
			tr->src, tr->dest, text_encode);

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
	
	ptr = realloc(mem->memory, mem->size + realsize +1);
       	if (ptr == NULL)
		die("write_callback()");

	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

static void
help(FILE *f)
{
	fprintf(f, "moetranslate - Simple language translator\n\n"
			"Usage: %s [-b] [SOURCE:TARGET] [TEXT]\n"
			"       -b        Brief mode\n"
			"       -f        Full mode\n"
			"       -h        Show this help\n\n"
			"Example:\n"
			"   Brief Mode:  %s -b en:id \"Hello\"\n"
			"   Full Mode :  %s -f id:en \"Halo\"\n"
			"   Auto Lang :  %s -f auto:en \"こんにちは\"\n",
		argv0, argv0, argv0, argv0);
}


int
main(int argc, char *argv[])
{
	argv0 = argv[0];
	int mode = arg_parse(argc, argv);

	if (mode == NONE)
		return EXIT_SUCCESS;
	else if (mode == ERR)
		return EXIT_FAILURE;

	Translate tr;

	tr.mode = mode;
	tr.src = argv[2];
	tr.dest = argv[3];
	tr.text = ltrim((rtrim(argv[4])));

	if (tr.mode == BRIEF)
		brief_mode(&tr);
	else
		full_mode(&tr);

	return EXIT_SUCCESS;
}

