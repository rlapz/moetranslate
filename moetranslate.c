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
enum { BRIEF, FULL, RAW, DETECT };

enum { NORMAL, INTERACTIVE };

typedef struct {
	char   *lcode,	   /* language code    */
	       *lang;
} Language;

typedef struct {
	char   *memory;
	size_t  size;
} Memory;

typedef struct {
	int     mode;	   /* mode translation */
	char   *src,	   /* source language  */
	       *target,    /* target language  */
	       *text;	   /* text/words       */
} Translate;

typedef struct {
	char   *base_url,
	       *params[4]; /* url parameter (brief, full, raw mode, detect lang) */
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
static void        raw_mode        (const cJSON *result);
static void        inter_input     (Translate *tr);
static void        help            (FILE *out);

/* config.h for applying patches and the configuration. */
#include "config.h"


/* function implementations */
static const char *
get_lang(const char *lcode)
{
	size_t lang_len = LENGTH(language);

	for (size_t i = 0; i < lang_len; i++) {
		if (strcmp(lcode, language[i].lcode) == 0)
			return language[i].lang;
	}

	return NULL;
}

static void
get_result(const Translate *tr)
{
	char    url[(TEXT_MAX_LEN * 3) + 150];
	CURL   *curl;
	cJSON  *result;
	Memory  mem = {0};

	curl = curl_easy_init();
	if (curl == NULL)
		DIE("get_result(): curl_easy_init()");

	url_parser(url, sizeof(url), tr);
	request_handler(&mem, curl, url);

	result = cJSON_Parse(mem.memory);
	if (result == NULL) {
		errno = EINVAL;
		DIE("get_result(): cJSON_Parse(): Parsing error!");
	}

	switch (tr->mode) {
	case BRIEF:
		brief_mode(result);
		break;
	case FULL:
		full_mode(tr, result);
		break;
	case RAW:
		raw_mode(result);
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
	int  ret  = -1,
	     mode = tr->mode;
	char text_encode[TEXT_MAX_LEN * 3];

	url_encode(text_encode, (unsigned char *)tr->text, sizeof(text_encode));

	switch (tr->mode) {
	case DETECT:
		ret = snprintf(dest, len, "%s%s&q=%s", 
				url_google.base_url, url_google.params[mode],
				text_encode);
		break;
	case BRIEF:
		ret = snprintf(dest, len, "%s%s&sl=%s&tl=%s&q=%s",
				url_google.base_url, url_google.params[mode],
				tr->src, tr->target, text_encode);
		break;
	case RAW :
		/* because raw and full mode has the same url */
		mode = FULL;
 		/* FALLTHROUGH */
	case FULL:
		ret = snprintf(dest, len, "%s%s&sl=%s&tl=%s&hl=%s&q=%s",
				url_google.base_url, url_google.params[mode],
				tr->src, tr->target, tr->target, text_encode);
		break;
	default:
		DIE("url_parser(): mode is invalid");
	}

	if (ret < 0)
		DIE("url_parser(): formatting url");

	return dest;
}

static void
request_handler(Memory *dest, CURL *curl, const char *url)
{
	CURLcode ccode;

	curl_easy_setopt(curl, CURLOPT_URL,           url            );
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback );
	curl_easy_setopt(curl, CURLOPT_WRITEDATA,     (Memory *)dest );
	curl_easy_setopt(curl, CURLOPT_USERAGENT,     user_agent     );
	curl_easy_setopt(curl, CURLOPT_TIMEOUT,       timeout        );

	ccode = curl_easy_perform(curl);
	if (ccode != CURLE_OK) {
		DIE_E("request_handler()", curl_easy_strerror(ccode));
	}
}

/* https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html */
static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	char   *ptr;
	Memory *mem      = (Memory *)data;
	size_t	realsize = (size * nmemb);

	ptr = realloc(mem->memory, mem->size + realsize +1);
	if (ptr == NULL)
		DIE("write_callback(): realloc");

	memcpy(ptr + mem->size, contents, realsize);

	mem->memory             = ptr;
	mem->size              += realsize;
	mem->memory[mem->size]  = '\0';

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
	char  *lang_c;
	cJSON *lang_src = result->child->next->next;

	if (cJSON_IsString(lang_src)) {
		lang_c = lang_src->valuestring;
		printf("%s (%s)\n", lang_c, get_lang(lang_c));
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
	cJSON *trans_text     = result->child;

	cJSON *examples       = cJSON_GetArrayItem(result, 13),
	      *definitions    = cJSON_GetArrayItem(result, 12),
	      *spelling       = cJSON_GetArrayItem(trans_text,
				 cJSON_GetArraySize(trans_text) -1);
	cJSON *src_correction = cJSON_GetArrayItem(result, 7),
	      *src_lang       = cJSON_GetArrayItem(result, 2),
	      *synonyms       = cJSON_GetArrayItem(result, 1);

	cJSON *src_spelling   = cJSON_GetArrayItem(spelling, 3),
	      *tgt_spelling   = cJSON_GetArrayItem(spelling, 2);


	cJSON *src_syn,  /* synonyms    */
	      *tgt_syn;
	cJSON *def_sub,  /* definitions */
	      *def_val,
	      *def_oth;
	cJSON *expl_val; /* examples    */	


	/* source text */
	printf("\"%s\"\n", tr->text);

	/* correction */
	if (cJSON_IsString(src_correction->child)) {
		printf("\n" YELLOW_BOLD_C "Did you mean: " END_C
			"\"%s\"" YELLOW_BOLD_C " ?\n\n" END_C,
			src_correction->child->next->valuestring);
	}

	/* source spelling */
	if (cJSON_IsString(src_spelling))
		printf("( " YELLOW_C "%s" END_C " )\n", src_spelling->valuestring);

	/* source lang */
	if (cJSON_IsString(src_lang)) {
		printf(GREEN_C "[ %s ]:" END_C " %s\n\n",
			src_lang->valuestring, get_lang(src_lang->valuestring));
	}

	/* target text */
	cJSON_ArrayForEach(i, trans_text) {
		if (cJSON_IsString(i->child))
			printf(WHITE_BOLD_C "%s" END_C, i->child->valuestring);
	}

	putchar('\n');

	/* target spelling */
	if (cJSON_IsString(tgt_spelling))
		printf("( " YELLOW_C "%s" END_C " )\n", tgt_spelling->valuestring);

	/* target lang */
	printf( GREEN_C "[ %s ]:" END_C " %s\n", tr->target, get_lang(tr->target));

	putchar('\n');

	/* synonyms */
	if (!cJSON_IsArray(synonyms) || synonym_max_line == 0)
		goto l_definitions;

	printf("\n%s", "------------------------");

	/* label */
	char *syn_lbl_str,
	     *tgt_syn_str;

	cJSON_ArrayForEach(i, synonyms) {
		int   iter    = 1,
		      syn_max = synonym_max_line;

		/* Verb, Noun, etc */
		syn_lbl_str    = i->child->valuestring;
		syn_lbl_str[0] = toupper(syn_lbl_str[0]);

		printf("\n" BLUE_BOLD_C "[ %s ]" END_C, syn_lbl_str);

		/* target alternatives */
		cJSON_ArrayForEach(tgt_syn, cJSON_GetArrayItem(i, 2)) {
			if (syn_max == 0)
				break;

			tgt_syn_str = tgt_syn->child->valuestring;
			tgt_syn_str[0] = toupper(tgt_syn_str[0]);

			printf("\n" WHITE_BOLD_C "%d. %s:" END_C "\n\t"
				YELLOW_C "-> " END_C, iter, tgt_syn_str);

			/* source alternatives */
			int syn_src_size = cJSON_GetArraySize(cJSON_GetArrayItem(
						tgt_syn, 1)) -1;

			cJSON_ArrayForEach(src_syn, cJSON_GetArrayItem(tgt_syn, 1)) {
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

	char *def_lbl_str, /* label */
	     *def_sub_str;

	cJSON_ArrayForEach(i, definitions) {
		int   iter    = 1,
		      def_max = definition_max_line;

		def_lbl_str = i->child->valuestring;

		if (strlen(def_lbl_str) == 0)
			continue;

		def_lbl_str[0] = toupper(def_lbl_str[0]);
		printf("\n" YELLOW_BOLD_C "[ %s ]" END_C, def_lbl_str);

		cJSON_ArrayForEach(def_sub, cJSON_GetArrayItem(i, 1)) {
			if (def_max == 0)
				break;

			def_val	= cJSON_GetArrayItem(def_sub, 2);
			def_oth	= cJSON_GetArrayItem(def_sub, 3);

			def_sub_str    = def_sub->child->valuestring;
			def_sub_str[0] = toupper(def_sub_str[0]);

			printf("\n" WHITE_BOLD_C "%d. %s" END_C "\n\t", iter, def_sub_str);

			if (cJSON_IsString(def_val))
				printf(YELLOW_C "->" END_C " %s ", def_val->valuestring);

			if (cJSON_IsArray(def_oth) && cJSON_IsString(def_oth->child->child))
				printf(GREEN_C "[ %s ]" END_C, def_oth->child->child->valuestring);

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

	int  iter     = 1,
	     expl_max = example_max_line;
	char *expl_str;
	cJSON_ArrayForEach(i, examples) {
		cJSON_ArrayForEach(expl_val, i) {
			if (expl_max == 0)
				break;

			expl_str = expl_val->child->valuestring;

			/* eliminating <b> ... </b> tags */
			trim_tag(expl_str, 'b');

			expl_str[0] = toupper(expl_str[0]);

			printf("%d. " YELLOW_C "%s" END_C "\n", iter, expl_str);

			iter++;
			expl_max--;
		}
		putchar('\n');
	}
}

static void
raw_mode(const cJSON *result)
{
	char *out = cJSON_Print(result);

	puts(out);

	free(out);
}

static void
inter_input(Translate *tr)
{
	printf(WHITE_BOLD_C
		"Interactive input mode" END_C "\n"
		"Max text length: %d\n\n", TEXT_MAX_LEN
	);

	char buffer[TEXT_MAX_LEN];

	while (1) {
		printf("Input text: ");
		fgets(buffer, TEXT_MAX_LEN, stdin);

		if (strlen(buffer) <= 1)
			break;

		tr->text = buffer;
		get_result(tr);

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
		"Usage: moetranslate [-b/-f/-r/-d/-h] [SOURCE] [TARGET] [TEXT]\n"
		"       -b         Brief mode\n"
		"       -f         Full mode\n"
		"       -r         Raw output (json)\n"
		"       -d         Detect language\n"
		"       -i         Interactive input\n"
		"       -h         Show this help\n\n"
		"Examples:\n"
		"   Brief Mode  :  moetranslate -b en:id \"Hello\"\n"
		"   Full Mode   :  moetranslate -f id:en \"Halo\"\n"
		"   Auto Lang   :  moetranslate -f auto:en \"こんにちは\"\n"
		"   Detect Lang :  moetranslate -d \"你好\"\n"
		"   Interactive :  moetranslate -i -f auto:en \"Hello\"\n"
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

	Translate t    = {0};
	int input_mode = NORMAL;

	if (argc == 3 && strcmp(argv[1], "-d") == 0) {
		t.mode = DETECT;
		t.text = rtrim(ltrim(argv[2]));
		goto result;
	}

	if (argc != 4)
		goto err;

	if (strcmp(argv[1], "-i") == 0) {
		input_mode = INTERACTIVE;
		argv += 1;
	}

	char *src    = strtok(argv[2], ":"),
	     *target = strtok(NULL,    ":");

	if (src == NULL || target == NULL)
		goto err;

	t.src    = src;
	t.target = target;

	if (input_mode == NORMAL)
		t.text = ltrim(rtrim(argv[3]));

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
	else if (strcmp(argv[1], "-r") == 0)
		t.mode = RAW;
	else
		goto err;

result:
	if (input_mode == NORMAL && strlen(t.text) >= TEXT_MAX_LEN) {
		fprintf(stderr, "Text too long, MAX length: %d characters\n",
				TEXT_MAX_LEN);
		goto err;
	}


	switch (input_mode) {
	case NORMAL:
		get_result(&t);
		break;
	case INTERACTIVE:
		inter_input(&t);
		break;
	default:
		goto err;
	}

	return EXIT_SUCCESS;

err:
	help(stderr);
	return EXIT_FAILURE;
}

