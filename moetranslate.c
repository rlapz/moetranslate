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

/* function declaration */
static void	 brief_mode	 (const cJSON *result);
static void    	 detect_lang	 (const cJSON *result);
static void    	 full_mode	 (const Translate *tr, cJSON *result);
static char    	*get_lang	 (const char *lcode);
static void    	 get_result	 (const Translate *tr);
static void    	 help		 (FILE *out);
static void	 request_handler (Memory *dest, CURL *curl, const char *url);
static char 	*url_parser	 (char *dest, size_t len, const Translate *tr);
static size_t  	 write_callback	 (char *ptr, size_t size, size_t nmemb, void *data);

/* config.h for applying patches and the configuration. */
#include "config.h"


/* function implementations */
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
	cJSON *i; /* iterator  */

	/* Source text */
	cJSON *source = result->child;
	cJSON *source_val;

	putchar('"');
	cJSON_ArrayForEach(i, source) {
		source_val = i->child->next;
		if (cJSON_IsString(source_val))
			printf("%s", source_val->valuestring);
	}
	puts("\"");
	

	/* Get correction 
	 *
	 * Did you mean: " text " ?
	 *
	 *------------------------------------
	 * Example:
	 *------------------------------------
	 *
	 * Did you mean: "behind me" ?
	 */
	cJSON *correction = cJSON_GetArrayItem(result, 7);

	if (cJSON_IsString(correction->child) &&
			cJSON_IsString(correction->child->next)) {
		printf("\n" YELLOW_BOLD_E "Did you mean: " END_E
				"\"%s\"" YELLOW_BOLD_E " ?\n\n" END_E,
				correction->child->next->valuestring);
	}


	/* Get spelling array index */
	cJSON *spelling = cJSON_GetArrayItem(result->child, 
				cJSON_GetArraySize(result->child) -1);


	/* Source spelling 
	 *
	 * ( spelling )
	 *
	 *------------------------------------
	 * Example:
	 *------------------------------------
	 *
	 * ( həˈlō )
	 */
	cJSON *spelling_src = cJSON_GetArrayItem(spelling, 3);

	if (cJSON_IsString(spelling_src)) {
		printf("( " YELLOW_E "%s" END_E " )\n", spelling_src->valuestring);
	}


	/* Source lang
	 *
	 * [ language code ]: language
	 *
	 *------------------------------------
	 * Example:
	 *------------------------------------
	 *
	 * [en]: English
	 */
	cJSON *lang_src = cJSON_GetArrayItem(result, 2);

	if (cJSON_IsString(lang_src)) {
		char *lang_src_str = lang_src->valuestring;
		printf(GREEN_E "[ %s ]:" END_E " %s\n\n", lang_src_str,
				get_lang(lang_src_str));
	}


	/* Target text */
	cJSON *target = result->child;
	cJSON *target_val;

	cJSON_ArrayForEach(i, target) {
		target_val = i->child;
		if (cJSON_IsString(target_val))
			printf(WHITE_BOLD_E "%s" END_E, target_val->valuestring);
	}
	putchar('\n');


	/* Target spelling 
	 *
	 * ( spelling )
	 *
	 *------------------------------------
	 * Example:
	 *------------------------------------
	 *
	 * ( həˈlō )
	 */
	cJSON *spelling_target = cJSON_GetArrayItem(spelling, 2);

	if (cJSON_IsString(spelling_target)) {
		printf("( " YELLOW_E "%s" END_E " )\n", spelling_target->valuestring);
	}


	/* Target lang
	 *
	 * [ language code ]: language
	 *
	 *------------------------------------
	 * Example:
	 *------------------------------------
	 *
	 * [id]: Indonesian
	 */
	printf( GREEN_E "[ %s ]:" END_E " %s\n",
			tr->target, get_lang(tr->target));


	/* Synonyms 
	 *
	 * [ label ] (depends on target language)
	 *   target word alternative:
	 *         -> list alternative words
	 *
	 *------------------------------------
	 * Example:
	 *------------------------------------
	 * NOTE: verba = verb
	 *
	 * [Verba]
	 *   Halo!:
	 *         -> Hello!
	 *   Salam!:
	 *         -> Hallo!, Holla!, Hello!
	 */
	cJSON *synonym = cJSON_GetArrayItem(result, 1);
	cJSON *syn_label,
	      *syn_src,
	      *syn_target;

	if (cJSON_IsArray(synonym))
		printf("\n%s", "------------------------");

	cJSON_ArrayForEach(i, synonym) {
		int max_syn = synonym_max_line;
		syn_label   = i->child; /* Verb, Noun, etc */

		printf("\n" BLUE_BOLD_E "[ %s ]" END_E, syn_label->valuestring);

		/* target word alternatives */
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
							syn_target, 1)) -1;
			cJSON_ArrayForEach(syn_src,
					cJSON_GetArrayItem(syn_target, 1)) {
				printf("%s", syn_src->valuestring);

				if (syn_src_size > 0) {
					printf(", ");
					syn_src_size--;
				}
			}
		}
		putchar('\n');
	}


	/* Examples 
	 *
	 * [ label ] (depends on target language)
	 *   explainations
	 *      -> examples
	 *
	 *------------------------------------
	 * Example:
	 *------------------------------------
	 * NOTE: kata seru = interjection, nomina = noun
	 *
	 * [ Kata seru ]
         *   used as a greeting or to begin a phone conversation.
	 *      -> hello there, Katie!
	 *
	 * [ Nomina ]
	 *   an utterance of “hello”; a greeting.
	 *      -> she was getting polite nods and hellos from people
	 */
	cJSON *example = cJSON_GetArrayItem(result, 12);
	cJSON *example_sub,
	      *example_desc,
	      *example_val,
	      *example_exp;

	if (cJSON_IsArray(example))
		printf("\n\n%s", "------------------------");

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

	/* More examples */
	cJSON *more_example = cJSON_GetArrayItem(result, 13);
	cJSON *more_example_a,
	      *more_example_val;

	if (cJSON_IsArray(more_example)) {
		printf("\n\n%s\n", "------------------------");

		int example_max = example_max_line;

		cJSON_ArrayForEach(i, more_example) {
			cJSON_ArrayForEach(more_example_a, i) {
				more_example_val = more_example_a->child;
				if (!cJSON_IsString(more_example_val) ||
						example_max == 0)
					break;
				if (example_max > 0)
					example_max--;

				/* eliminating <b> ... </b> tags */
				trim_tag(more_example_val->valuestring, 'b');
				printf("\"" YELLOW_E "%s" END_E "\"\n",
						more_example_val->valuestring);
			}
		}
		putchar('\n');
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
get_result(const Translate *tr)
{
	char	 url[(TEXT_MAX_LEN * 3) + 150] = {0};
	CURL	*curl;
	cJSON	*result;
	Memory	 mem  = {NULL, 0};

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

static void
request_handler(Memory *dest, CURL *curl, const char *url)
{
	CURLcode ccode;

	curl_easy_setopt(curl, CURLOPT_URL,           url	    );
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA,     (void*)dest   );
	curl_easy_setopt(curl, CURLOPT_USERAGENT,     user_agent    );
	curl_easy_setopt(curl, CURLOPT_TIMEOUT,       timeout       );

	ccode = curl_easy_perform(curl);
	if (ccode != CURLE_OK)
		die("request_handler(): %s", curl_easy_strerror(ccode));
}

static char *
url_parser(char *dest, size_t len, const Translate *tr)
{
	int   ret;
	char  text_encode[TEXT_MAX_LEN * 3];

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

/* https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html */
static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	char   *ptr;
	Memory *mem	   = (Memory *)data;
	size_t	realsize   = (size * nmemb);

	ptr = realloc(mem->memory, mem->size + realsize +1);
       	if (ptr == NULL)
		die("write_callback(): realloc");

	memcpy(ptr + mem->size, contents, realsize);

	mem->memory	       = ptr;
	mem->size             += realsize;
	mem->memory[mem->size] = '\0';

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

