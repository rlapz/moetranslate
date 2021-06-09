/* MIT License
 *
 * Copyright (c) 2021 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <curl/curl.h>

#include "lib/cJSON.h"


/* macros */
#define STRING_NEW() (calloc(sizeof(char), sizeof(char)))
#define LENGTH(X) (sizeof(X) / sizeof(X[0]))

#define BRIEF	0	/* brief mode */
#define FULL	1	/* full mode */

struct Translate {
	int	mode;	/* mode translation */
	char	*src;	/* source language */
	char	*dest;	/* target language */
	char	*text;	/* text/words */
};

struct Memory {
	char	*memory;
	size_t	size;
};

/* function declaration */
static char *ltrim(const char *str);
static char *rtrim(char *str);
static char *get_lang(const char *lcode);
static void brief_mode(void);
static void full_mode(void);
static char *url_parser(CURL *curl);
static char *request_handler(void);
static char *html_cleaner(char **dest);
static char *string_append(char **dest, const char *fmt, ...);
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *data);

/* config.h for applying patches and the configuration. */
#include "config.h"

/* global variables */
static struct Translate tr;

/* function implementations */
/* left trimming */
static char *
ltrim(const char *str)
{
	while (*str && isspace((unsigned char)(*str)))
		str++;
	return (char*)str;
}

/* right trimming */
static char *
rtrim(char *str)
{
	char *end = str + strlen(str) -1;
	while (end > str && isspace((unsigned char)(*end))) {
		*end = '\0';
		end--;
	}
	return str;
}

static char *
get_lang(const char *lcode)
{
	size_t lcode_len = strlen(lcode);
	for (size_t i = 0; i < LENGTH(lang_code); i++) {
		if (strncmp(lcode, lang_code[i][0], lcode_len) == 0)
			return (char*)lang_code[i][1];
	}
	return NULL;
}

static void
brief_mode(void)
{
	char *req_str = NULL;
	cJSON *parser, *iterator, *value;

	if (!(req_str = request_handler()))
		exit(1);

	/* cJSON parser */
	if (!(parser = cJSON_Parse(req_str))) {
		perror("brief_mode(): cJSON_Parse()");
		goto cleanup;
	}

	/* dest[i][0][0] */
	cJSON_ArrayForEach(iterator, cJSON_GetArrayItem(parser,0)) {
		value = cJSON_GetArrayItem(iterator, 0); /* index: 0 */
		if (cJSON_IsString(value))
			/* send the result to stdout */
			fprintf(stdout, "%s", cJSON_GetStringValue(value));
	}
	puts("");

cleanup:
	cJSON_Delete(parser);
	free(req_str);
}

static void
full_mode(void)
{
	char *req_str		= NULL;
	char *trans_src		= NULL;
	char *trans_dest	= NULL;
	char *spell_str		= NULL;
	char *lang_str		= NULL;
	char *correct_str	= NULL;
	char *syn_str		= NULL;
	char *example_str	= NULL;

	cJSON *parser, *iterator;
	cJSON *trans, *spell, *synonym, *correct, *example, *langdest;
	cJSON *trans_val, *syn_val1, *syn_val2, *example_val;

	/* init curl session */
	if (!(req_str = request_handler()))
		exit(1);
	
	/* test  */
	//fprintf(stdout, "%s\n", req_str);

	/* cJSON parser */
	if (!(parser = cJSON_Parse(req_str))) {
		perror("full_mode(): cJSON_Parse()");
		goto cleanup;
	}

	/* cJSON Parser */
	/* get translation */
	uint8_t count_tr = 0;
	trans = cJSON_GetArrayItem(parser, 0);
	if (!(trans_src = STRING_NEW()))
		goto cleanup;
	if (!(trans_dest = STRING_NEW()))
		goto cleanup;
	cJSON_ArrayForEach(iterator, trans) {
		trans_val = cJSON_GetArrayItem(iterator, 0);
		if (cJSON_IsString(trans_val)) {
			string_append(&trans_dest, "\033[1m\033[37m%s\033[0m",
					trans_val->valuestring);
			string_append(&trans_src, "%s",
					trans_val->next->valuestring);
		}
		count_tr++;
	}

	/* get spelling */
	uint8_t count_spell = 0;
	spell = cJSON_GetArrayItem(cJSON_GetArrayItem(parser, 0), count_tr -1);
	if (!(spell_str = STRING_NEW()))
		goto cleanup;
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
	correct = cJSON_GetArrayItem(parser, 7);
	if (!(correct_str = STRING_NEW()))
		goto cleanup;
	if (cJSON_IsString(correct->child)) {
		free(trans_src);
		if (!(trans_src = STRING_NEW()))
			goto cleanup;
		string_append(&correct_str, "\nDid you mean: \"%s\"?\n",
				correct->child->next->valuestring);
		string_append(&trans_src, correct->child->next->valuestring);
	}

	/* get language */
	char *lang_v = NULL;
	langdest = cJSON_GetArrayItem(parser, 2);
	if (!(lang_str = STRING_NEW()))
		goto cleanup;
	if (cJSON_IsString(langdest)) {
		lang_v = get_lang(langdest->valuestring);
		string_append(&lang_str, "\n[%s]: %s",
				langdest->valuestring,
				lang_v ? lang_v : "");
	}

	/* get synonyms */
	char *syn_tmp;
	uint8_t count_syn = 0;
	synonym = cJSON_GetArrayItem(parser, 1);
	if (!(syn_str = STRING_NEW()))
		goto cleanup;
	cJSON_ArrayForEach(iterator, synonym) {
		syn_tmp = iterator->child->valuestring;
		syn_tmp[0] = toupper(syn_tmp[0]);

		if (count_syn > 0)
			string_append(&syn_str, "\n");

		string_append(&syn_str, "\n\033[1m\033[37m[%s]:\033[0m",
				syn_tmp);

		cJSON_ArrayForEach(syn_val1, cJSON_GetArrayItem(iterator, 2)) {
			string_append(&syn_str, "\n  \033[1m\033[37m%s:\033[0m\n",
					syn_val1->child->valuestring);
			string_append(&syn_str, "\t");
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
	uint8_t max = 5; /* examples max */
	example = cJSON_GetArrayItem(parser, 13);
	if (!(example_str = STRING_NEW()))
		goto cleanup;
	if (!cJSON_IsNull(example)) {
		string_append(&example_str, "\n%s\n",
				"------------------------------------");
		cJSON_ArrayForEach(iterator, example ) {
			cJSON_ArrayForEach(example_val, iterator) {
				if (max == 0)
					break;
				string_append(&example_str, "\"%s\"\n",
						example_val->child->valuestring);
				max--;
			}
		}
		html_cleaner(&example_str);
	}
	
	/* output */
	/* experimental */
	if (strlen(trans_src) == 0)
		goto cleanup;

	/* print to stdout */
	fprintf(stdout, "%s\"%s\"%s\n\n%s\n[%s]: %s\n%s%s%s",
			correct_str,
			trans_src, lang_str, trans_dest,
			tr.dest, get_lang(tr.dest),
			spell_str, syn_str, example_str);

cleanup:
	cJSON_Delete(parser);
	if (req_str)
		free(req_str);
	if (trans_src)
		free(trans_src);
	if (trans_dest)
		free(trans_dest);
	if (spell_str)
		free(spell_str);
	if (lang_str)
		free(lang_str);
	if (syn_str)
		free(syn_str);
	if (correct_str)
		free(correct_str);
	if (example_str)
		free(example_str);
}

static char *
url_parser(CURL *curl)
{
	char *ret		= NULL;
	char *text_encode	= NULL;

	/* text encoding */
	text_encode = curl_easy_escape(curl, tr.text, (int)strlen(tr.text));
	if (!text_encode) {
		perror("url_parser(): curl_easy_escape()");
		return NULL;
	}

	if (!(ret = STRING_NEW()))
		goto cleanup;

	string_append(&ret, "%s", url_google);
	string_append(&ret, url_params[tr.mode],
			tr.src, tr.dest, text_encode);

cleanup:
	curl_free(text_encode);

	return ret;
}

static char *
request_handler(void)
{
	char *url		= NULL;
	CURL *curl		= NULL;
	struct Memory mem	= {.memory = NULL, .size = 0};
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

static char *
html_cleaner(char **dest)
{
	char *p		= (*dest);
	char *tmp	= NULL;
	size_t i	= 0;
	
	if (!(tmp = calloc(sizeof(char), strlen(*dest)+1)))
		return (*dest);

	/* UNSAFE
	 * can caused segfault
	 */
	while (*p) {
		if (*p == '<' && *(p+1) == 'b' && *(p+2) == '>')
			p += 3;
		else if (*p == '<' && *(p+1) == '/' && *(p+2) == 'b' &&
			       	*(p+3) == '>')
			p += 4;
		tmp[i] = (*p);
		i++;
		p++;
	}
	free(*dest);
	(*dest) = tmp;

	return (*dest);
}

static char *
string_append(char **dest, const char *fmt, ...)
{
	char *tmp_p	= NULL;
	char *tmp_dest	= NULL;
	int n		= 0;
	size_t size	= 0;
	va_list vargs;

	/* determine required size */
	va_start(vargs, fmt);
	n = (size_t)vsnprintf(tmp_p, size, fmt, vargs);
	va_end(vargs);

	if (n < 0)
		goto cleanup;

	size = (size_t)n +1; /* one extra byte for '\0' */
	if (!(tmp_p = malloc(size)))
		goto cleanup;

	va_start(vargs, fmt);
	n = vsnprintf(tmp_p, size, fmt, vargs);
	va_end(vargs);

	if (n < 0)
		goto cleanup;

	if (!(tmp_dest = realloc((*dest), (strlen((*dest)) + size))))
		goto cleanup;

	(*dest) = tmp_dest;
	strncat((*dest), tmp_p, size -1);

cleanup:
	if (tmp_p)
		free(tmp_p);
	return (*dest);
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
		return EXIT_FAILURE;
	}

	tr.src = argv[1];
	tr.dest = argv[2];
	tr.mode = BRIEF;

	if (strncmp(tr.src, "auto", 5) != 0) {
		if (!get_lang(tr.src)) {
			fprintf(stderr, "Unknown \"%s\" language code\n", tr.src);
			return EXIT_FAILURE;
		}
	}
	if (!get_lang(tr.dest)) {
		fprintf(stderr, "Unknown \"%s\" language code\n", tr.dest);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[3], "-b") == 0) {
		if (argv[4] == NULL || strlen(argv[4]) == 0) {
			fprintf(stderr, help, argv[0], argv[0], argv[0]);
			return EXIT_FAILURE;
		}
		tr.text = argv[4];
	} else {
		tr.text = argv[3];
		tr.mode = FULL;
	}

	tr.text = ltrim((rtrim(tr.text)));
	if (tr.mode == FULL)
		full_mode();
	else
		brief_mode();


	return EXIT_SUCCESS;
}

