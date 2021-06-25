/* MIT License
 *
 * Copyright (c) 2021 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "util.h"
#include "cJSON.h"


/* macros */
#define BRIEF	0	/* brief mode */
#define FULL	1	/* full mode */

typedef struct {
	int	mode;	/* mode translation */
	char	*src;	/* source language */
	char	*dest;	/* target language */
	char	*text;	/* text/words */
} Translate;

struct Memory {
	char	*memory;
	size_t	size;
};

/* function declaration */
static void brief_mode(Translate *tr);
static void full_mode(Translate *tr);
static char *get_lang(const char *lcode);
static char *request_handler(Translate *tr);
static char *url_parser(Translate *tr, CURL *curl);
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *data);

/* config.h for applying patches and the configuration. */
#include "config.h"

/* function implementations */
static void
brief_mode(Translate *tr)
{
	char *req_str = NULL;
	cJSON *parser, *iterator;

	if ((req_str = request_handler(tr)) == NULL)
		die("brief_mode(): request_handler()");

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
			fprintf(stdout, "%s", cJSON_GetStringValue(value));
	}
	puts("");

	cJSON_Delete(parser);
	free(req_str);
}

static void
full_mode(Translate *tr)
{
	char *req_str = NULL;
	cJSON *parser, *iterator;

	/* init curl session */
	if ((req_str = request_handler(tr)) == NULL)
		die("full_mode() : request_handler()");
	
	/* test  */
	// fprintf(stdout, "%s\n", req_str);

	/* cJSON parser */
	if ((parser = cJSON_Parse(req_str)) == NULL) {
		errno = EINVAL;
		die("full_mode(): cJSON_Parse(): Parsing error!");
	}

	/* cJSON Parser */
	/* get translation */
	char *trans_src		= STRING_NEW();
	char *trans_dest	= STRING_NEW();
	uint8_t count_tr	= 0;
	cJSON *trans		= cJSON_GetArrayItem(parser, 0);
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
	char *spell_str		= STRING_NEW();
	uint8_t count_spell	= 0;
	cJSON *spell		= cJSON_GetArrayItem(cJSON_GetArrayItem(parser, 0),
							count_tr -1);
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
	if (cJSON_IsString(correct->child)) {
		free(trans_src);
		trans_src = STRING_NEW();
		string_append(&correct_str,
				"\n\033[1m\033[37mDid you mean: \033[0m\"%s\"?\n",
				correct->child->next->valuestring);
		string_append(&trans_src, correct->child->next->valuestring);
	}

	/* get language */
	char *lang_v		= NULL;
	char *lang_str		= STRING_NEW();
	cJSON *langdest		= cJSON_GetArrayItem(parser, 2);
	if (cJSON_IsString(langdest)) {
		lang_v = get_lang(langdest->valuestring);
		string_append(&lang_str, "\n[%s]: %s",
				langdest->valuestring,
				lang_v ? lang_v : "");
	}

	/* get synonyms */
	char *syn_str		= STRING_NEW();
	char *syn_tmp		= NULL;
	uint8_t count_syn	= 0;
	cJSON *synonym		= cJSON_GetArrayItem(parser, 1);
	cJSON_ArrayForEach(iterator, synonym) {
		syn_tmp = iterator->child->valuestring;
		syn_tmp[0] = toupper(syn_tmp[0]);

		if (count_syn > 0)
			string_append(&syn_str, "\n");

		string_append(&syn_str, "\n\033[1m\033[37m[%s]:\033[0m",
				syn_tmp);

		cJSON *syn_val1;
		cJSON_ArrayForEach(syn_val1, cJSON_GetArrayItem(iterator, 2)) {
			string_append(&syn_str, "\n  \033[1m\033[37m%s:\033[0m\n",
					syn_val1->child->valuestring);
			string_append(&syn_str, "\t");

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
	uint8_t max		= example_max_line;
	cJSON *example		= cJSON_GetArrayItem(parser, 13);

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

	if (trans_src == NULL || trans_dest == NULL ||
			req_str == NULL || spell_str == NULL ||
			lang_str == NULL || syn_str == NULL ||
			correct_str == NULL || example_str == NULL) {
		die("full_mode()");
	}
	
	/* output */
	/* print to stdout */
	fprintf(stdout, "%s\"%s\"%s\n\n%s\n[%s]: %s\n%s%s%s",
			correct_str,
			trans_src, lang_str, trans_dest,
			tr->dest, get_lang(tr->dest),
			spell_str, syn_str, example_str);

	free(req_str);
	free(trans_src);
	free(trans_dest);
	free(spell_str);
	free(lang_str);
	free(syn_str);
	free(correct_str);
	free(example_str);

	cJSON_Delete(parser);
}

static char *
get_lang(const char *lcode)
{
	size_t lang_len = LENGTH(lang_code);
	for (size_t i = 0; i < lang_len; i++) {
		if (strncmp(lcode, lang_code[i][0], 5) == 0)
			return (char*)lang_code[i][1];
	}
	return NULL;
}

static char *
request_handler(Translate *tr)
{
	char *url		= NULL;
	CURL *curl		= NULL;
	struct Memory mem	= {.memory = NULL, .size = 0};
	CURLcode ccode;

	/* curl init */
	if ((curl = curl_easy_init()) == NULL) {
		perror("request_handler(): curl_easy_init()");
		return NULL;
	}
	/* url parser */
	if ((url = url_parser(tr, curl)) == NULL) {
		perror("request_handler(): url_parser()");
		goto cleanup;
	}
	if ((mem.memory = malloc(1)) == NULL) {
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
	free(url);

	return mem.memory;
}

static char *
url_parser(Translate *tr, CURL *curl)
{
	char *ret		= STRING_NEW();
	char *text_encode	= NULL;

	/* text encoding */
	text_encode = curl_easy_escape(curl, tr->text, (int)strlen(tr->text));
	if (text_encode == NULL) {
		perror("url_parser(): curl_easy_escape()");
		return NULL;
	}

	string_append(&ret, "%s%s&sl=%s&tl=%s&q=%s",
			url_google, url_params[tr->mode],
			tr->src, tr->dest, text_encode);

	curl_free(text_encode);
	return ret;
}

/* https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html */
static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	char *ptr		= NULL;
	struct Memory *mem	= (struct Memory*)data;
	size_t realsize		= (size * nmemb);
	
	ptr = realloc(mem->memory, mem->size + realsize +1);
       	if (ptr == NULL) {
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
		return EXIT_FAILURE;
	}
	if (strcmp(argv[1], "auto") != 0 && get_lang(argv[1]) == NULL) {
		fprintf(stderr, "Unknown \"%s\" source language code\n", argv[1]);
		return EXIT_FAILURE;
	}
	if (strcmp(argv[2], "auto") == 0 || get_lang(argv[2]) == NULL) {
		fprintf(stderr, "Unknown \"%s\" target language code\n", argv[2]);
		return EXIT_FAILURE;
	}

	Translate tr;

	tr.src = argv[1];
	tr.dest = argv[2];
	if (strcmp(argv[3], "-b") == 0) {
		if (argv[4] == NULL || strlen(argv[4]) == 0) {
			fprintf(stderr, help, argv[0], argv[0], argv[0]);
			return EXIT_FAILURE;
		}
		tr.text = argv[4];
		tr.mode = BRIEF;
	} else {
		tr.text = argv[3];
		tr.mode = FULL;
	}

	tr.text = ltrim((rtrim(tr.text)));
	tr.mode == FULL ? full_mode(&tr) : brief_mode(&tr);

	return EXIT_SUCCESS;
}

