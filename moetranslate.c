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

#include "lib/cJSON.h"
#include "lib/linenoise.h"
#include "lib/util.h"

/* macros */
#define PRINT_SEP_1()  puts("------------------------")
#define PRINT_SEP_2()  printf("\n------------------------")

typedef enum {
	NORMAL, INTERACTIVE, PIPE
} InputText;

typedef enum {
	BRIEF, DETAIL, RAW, DETECT_LANG 
} DisplayText;

typedef struct {
	char   *value[2];    /* code and language */
} Lang;

typedef struct {
	char   *url, *text;
	struct {
		DisplayText disp;
		InputText input;
	} io;
	const  Lang *lang[2]; /* source and target */
	cJSON  *json;
} Translate;

typedef struct {
	char   *value;
	size_t  size;
} Memory;

/* function declaration, ordered in logical manner */
static const Lang *get_lang           (const char *lcode);
static int         set_lang           (Translate *tr, char *lcodes);
static void        interactive_mode   (Translate *tr);
static void        run                (Translate *tr);

static char       *url_parser         (Translate *tr, size_t len);
static void        req_handler        (Memory *dest, CURL *curl, const char *url);
static size_t      write_callback     (char *contents, size_t size, size_t nmemb,
                                       void *data);

static void        brief_output       (Translate *tr);
static void        detail_output      (Translate *tr);
static void        raw_output         (Translate *tr);
static void        detect_lang_output (Translate *tr);
static void        help               (FILE *in);

/* config.h for applying patches and the configuration. */
#include "config.h"

/* color functions */
#define REGULAR_GREEN(TEXT)  "\033[00;" GREEN_COLOR  "m" TEXT "\033[00m"
#define REGULAR_YELLOW(TEXT) "\033[00;" YELLOW_COLOR "m" TEXT "\033[00m"

#define BOLD_BLUE(TEXT)      "\033[01;" BLUE_COLOR   "m" TEXT "\033[00m"
#define BOLD_GREEN(TEXT)     "\033[01;" GREEN_COLOR  "m" TEXT "\033[00m"
#define BOLD_WHITE(TEXT)     "\033[01;" WHITE_COLOR  "m" TEXT "\033[00m"
#define BOLD_YELLOW(TEXT)    "\033[01;" YELLOW_COLOR "m" TEXT "\033[00m"


/* function implementations */
static void
interactive_mode(Translate *tr)
{
	printf( BOLD_WHITE("---[ Moetranslate ]---")
		"\n"
		BOLD_YELLOW("Interactive input mode")
		"\n"
		"\nChange language: /c [SOURCE]:[TARGET]\n"
		"Press Ctrl-d or type \"/q\" to exit.\n\n"
	);

	PRINT_SEP_1();

	linenoiseHistoryLoad(HISTORY_FILE_NAME);

	char *p;
	char *prompt = "[Input text] -> ";
	while ((p = linenoise(prompt)) != NULL) {
		linenoiseHistoryAdd(p);

		if (strcmp(p, "/q") == 0) {
			free(p);
			break;
		}

		if (strncmp(p, "/c", 2) == 0) {
			char *tmp = p;
			tmp += 2;

			if (set_lang(tr, tmp) < 0) {
				perror(NULL);
				continue;
			}

			printf("\nLanguage changed: "
					REGULAR_GREEN("[%s]") " %s -> "
					REGULAR_GREEN("[%s]") " %s\n\n",
					tr->lang[0]->value[0],
					tr->lang[0]->value[1],
					tr->lang[1]->value[0],
					tr->lang[1]->value[1]
			);

			free(p);
			continue;
		}

		tr->text = rtrim(ltrim(p));

		if (strlen(tr->text) == 0)
			continue;

		PRINT_SEP_1();

		run(tr);

		PRINT_SEP_1();

		free(p);
	}
	puts("\nExiting...");
}

static void
run(Translate *tr)
{	
	char url[(TEXT_MAX_LEN *3) + sizeof(URL_DETAIL)]; /* longest url */
	Memory mem = {0};
	CURL *curl;

	curl = curl_easy_init();
	if (curl == NULL)
		DIE("run(): curl_easy_init()");

	tr->url = url;
	url_parser(tr, sizeof(url));

	req_handler(&mem, curl, tr->url);

	tr->json = cJSON_Parse(mem.value);
	if (tr->json == NULL) {
		errno = EINVAL;
		DIE("run(): cJSON_Parse(): Parsing error!");
	}

	switch (tr->io.disp) {
	case DETECT_LANG:
		detect_lang_output(tr);
		break;
	case BRIEF:
		brief_output(tr);
		break;
	case RAW:
		raw_output(tr);
		break;
	case DETAIL:
		detail_output(tr);
		break;
	}

	free(mem.value);
	cJSON_Delete(tr->json);
	curl_easy_cleanup(curl);
}

static const Lang *
get_lang(const char *lcode)
{
	size_t len = LENGTH(lang);

	for (size_t i = 0; i < len; i++) {
		if (strcmp(lcode, lang[i].value[0]) == 0)
			return &lang[i];
	}

	return NULL;
}

static int
set_lang(Translate *tr, char *lcodes)
{
#define LANG_ERR(X) \
		fprintf(stderr, "Unknown \"%s\" language code.\n", X);

	char *src    = strtok(lcodes, ":");
	char *target = strtok(NULL,   ":");

	if (src == NULL || target == NULL)
		goto err;

	src    = rtrim(ltrim(src));
	target = rtrim(ltrim(target));

	if (strcmp(target, "auto") == 0) {
		LANG_ERR(target);
		goto err;
	}

	if ((tr->lang[0] = get_lang(src)) == NULL) {
		LANG_ERR(src);
		goto err;
	}

	if ((tr->lang[1] = get_lang(target)) == NULL) {
		LANG_ERR(target);
		goto err;
	}

	return 0;

err:
	errno = EINVAL;
	return -errno;
}

static char *
url_parser(Translate *tr, size_t len)
{
	int ret = -1;
	char text_enc[TEXT_MAX_LEN *3];

	url_encode(text_enc, (unsigned char *)tr->text, sizeof(text_enc));

	switch (tr->io.disp) {
	case DETECT_LANG:
		ret = snprintf(tr->url, len, URL_DETECT_LANG, text_enc);
		break;
	case BRIEF:
		ret = snprintf(tr->url, len, URL_BRIEF,
				tr->lang[0]->value[0], tr->lang[1]->value[0],
				text_enc);
		break;
	case RAW:
		/* because raw and detail output has the same url */
		/* FALLTHROUGH */
	case DETAIL:
		ret = snprintf(tr->url, len, URL_DETAIL,
				tr->lang[0]->value[0], tr->lang[1]->value[0],
				tr->lang[1]->value[0], text_enc);
		break;
	};

	if (ret < 0)
		DIE("url_parser()");

	return tr->url;
}

static void
req_handler(Memory *dest, CURL *curl, const char *url)
{
	CURLcode ccode;

	curl_easy_setopt(curl, CURLOPT_URL,           url            );
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback );
	curl_easy_setopt(curl, CURLOPT_WRITEDATA,     (Memory *)dest );
	curl_easy_setopt(curl, CURLOPT_USERAGENT,     USER_AGENT     );
	curl_easy_setopt(curl, CURLOPT_TIMEOUT,       TIMEOUT        );

	ccode = curl_easy_perform(curl);
	if (ccode != CURLE_OK) {
		DIE_E("req_handler()", curl_easy_strerror(ccode));
	}
}

/* https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html */
static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	char   *ptr;
	Memory *mem      = (Memory *)data;
	size_t	realsize = (size * nmemb);

	ptr = realloc(mem->value, mem->size + realsize +1);
	if (ptr == NULL)
		DIE("write_callback(): realloc");

	memcpy(ptr + mem->size, contents, realsize);

	mem->value             = ptr;
	mem->size              += realsize;
	mem->value[mem->size]  = '\0';

	return realsize;
}

static void
brief_output(Translate *tr)
{
	cJSON *i, *value;

	cJSON_ArrayForEach(i, (tr->json)->child) {
		value = i->child; /* index: 0 */
		if (cJSON_IsString(value))
			/* send the result to stdout */
			printf("%s", value->valuestring);
	}
	putchar('\n');
}

static void
detect_lang_output(Translate *tr)
{
	/* faster than cJSON_GetArrayItem(result, 2)
	 * (without iterations) but UNSAFE */ 
	char  *lang_c;
	cJSON *lang_src = (tr->json)->child->next->next;

	if (cJSON_IsString(lang_src)) {
		lang_c = lang_src->valuestring;
		printf("%s (%s)\n", lang_c, get_lang(lang_c)->value[1]);
	}
}

static void
detail_output(Translate *tr)
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

	cJSON *i;                          /* iterator      */
	cJSON *result         = tr->json;  /* temporary var */

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
		printf("\n" BOLD_YELLOW("Did you mean: ")
			"\"%s\"" BOLD_YELLOW(" ?") "\n\n",
			src_correction->child->next->valuestring);
	}

	/* source spelling */
	if (cJSON_IsString(src_spelling))
		printf("( " REGULAR_YELLOW("%s") " )\n", src_spelling->valuestring);

	/* source lang */
	if (cJSON_IsString(src_lang)) {
		printf(REGULAR_GREEN("[ %s ]:") " %s\n\n",
			src_lang->valuestring,
			get_lang(src_lang->valuestring)->value[1]);
	}

	/* target text */
	cJSON_ArrayForEach(i, trans_text) {
		if (cJSON_IsString(i->child))
			printf(BOLD_WHITE("%s"), i->child->valuestring);
	}

	putchar('\n');

	/* target spelling */
	if (cJSON_IsString(tgt_spelling))
		printf("( " REGULAR_YELLOW("%s") " )\n", tgt_spelling->valuestring);

	/* target lang */
	printf(REGULAR_GREEN("[ %s ]:") " %s\n", 
			tr->lang[1]->value[0], tr->lang[1]->value[1]);

	putchar('\n');

	/* synonyms */
	if (!cJSON_IsArray(synonyms) || SYNONYM_MAX_LINE == 0)
		goto l_definitions;

	PRINT_SEP_2();

	/* label */
	char *syn_lbl_str,
	     *tgt_syn_str;

	cJSON_ArrayForEach(i, synonyms) {
		int   iter    = 1,
		      syn_max = SYNONYM_MAX_LINE;

		/* Verb, Noun, etc */
		syn_lbl_str    = i->child->valuestring;
		syn_lbl_str[0] = toupper(syn_lbl_str[0]);

		printf("\n" BOLD_BLUE("[ %s ]"), syn_lbl_str);

		/* target alternatives */
		cJSON_ArrayForEach(tgt_syn, cJSON_GetArrayItem(i, 2)) {
			if (syn_max == 0)
				break;

			tgt_syn_str = tgt_syn->child->valuestring;
			tgt_syn_str[0] = toupper(tgt_syn_str[0]);

			printf("\n" BOLD_WHITE("%d. %s:") "\n\t"
				REGULAR_YELLOW("-> "), iter, tgt_syn_str);

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
	if (!cJSON_IsArray(definitions) || DEFINITION_MAX_LINE == 0)
		goto l_example;

	PRINT_SEP_2();

	char *def_lbl_str, /* label */
	     *def_sub_str;

	cJSON_ArrayForEach(i, definitions) {
		int   iter    = 1,
		      def_max = DEFINITION_MAX_LINE;

		def_lbl_str = i->child->valuestring;

		if (strlen(def_lbl_str) == 0)
			continue;

		def_lbl_str[0] = toupper(def_lbl_str[0]);
		printf("\n" BOLD_YELLOW("[ %s ]"), def_lbl_str);

		cJSON_ArrayForEach(def_sub, cJSON_GetArrayItem(i, 1)) {
			if (def_max == 0)
				break;

			def_val	= cJSON_GetArrayItem(def_sub, 2);
			def_oth	= cJSON_GetArrayItem(def_sub, 3);

			def_sub_str    = def_sub->child->valuestring;
			def_sub_str[0] = toupper(def_sub_str[0]);

			printf("\n" BOLD_WHITE("%d. %s") "\n\t", iter, def_sub_str);

			if (cJSON_IsString(def_val))
				printf(REGULAR_YELLOW("->") " %s ", def_val->valuestring);

			if (cJSON_IsArray(def_oth) && cJSON_IsString(def_oth->child->child))
				printf(REGULAR_GREEN("[ %s ]"), def_oth->child->child->valuestring);

			iter++;
			def_max--;
		}
		putchar('\n');
	}
	putchar('\n');

l_example:
	if (!cJSON_IsArray(examples) || EXAMPLE_MAX_LINE == 0)
		return; /* it's over */

	PRINT_SEP_2();
	putchar('\n');

	int  iter     = 1,
	     expl_max = EXAMPLE_MAX_LINE;
	char *expl_str;
	cJSON_ArrayForEach(i, examples) {
		cJSON_ArrayForEach(expl_val, i) {
			if (expl_max == 0)
				break;

			expl_str = expl_val->child->valuestring;

			/* eliminating <b> ... </b> tags */
			trim_tag(expl_str);

			expl_str[0] = toupper(expl_str[0]);

			printf("%d. " REGULAR_YELLOW("%s") "\n", iter, expl_str);

			iter++;
			expl_max--;
		}
		putchar('\n');
	}
}

static void
raw_output(Translate *tr)
{
	char *out = cJSON_Print(tr->json);

	puts(out);

	free(out);
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
		"       -b         Brief output\n"
		"       -f         Full/detail output\n"
		"       -r         Raw output (json)\n"
		"       -d         Detect language\n"
		"       -i         Interactive input mode\n"
		"       -h         Show this help\n\n"
		"Examples:\n"
		"   Brief Mode  :  moetranslate -b en:id \"Hello\"\n"
		"   Full Mode   :  moetranslate -f id:en \"Halo\"\n"
		"   Auto Lang   :  moetranslate -f auto:en \"こんにちは\"\n"
		"   Detect Lang :  moetranslate -d \"你好\"\n"
		"   Interactive :  moetranslate -i -f auto:en\n"
	);
}

int
main(int argc, char *argv[])
{
	Translate tr = { .io.input = NORMAL };


	/* dumb arg parser */
	if (argc == 2 && strcmp(argv[1], "-h") == 0) {
		help(stdout);
		return EXIT_SUCCESS;
	}

	if (argc == 3 && strcmp(argv[1], "-d") == 0) {
		tr.io.disp = DETECT_LANG;
		tr.text    = rtrim(ltrim(argv[2]));
		goto run_tr;
	}

	if (argc != 4)
		goto err;

	if (strcmp(argv[1], "-i") == 0) {
		argv += 1;
		tr.io.input = INTERACTIVE;
	}

	if (strcmp(argv[1], "-b") == 0)
		tr.io.disp = BRIEF;
	else if (strcmp(argv[1], "-f") == 0)
		tr.io.disp = DETAIL;
	else if (strcmp(argv[1], "-r") == 0)
		tr.io.disp = RAW;
	else
		goto err;

	if (set_lang(&tr, argv[2]) < 0)
		goto err;

	tr.text = rtrim(ltrim(argv[3]));

run_tr:
	if (tr.io.input != INTERACTIVE && 
			strlen(tr.text) >= TEXT_MAX_LEN) {
		fprintf(stderr, "Text too long, MAX length: %d characters, "
				"see: config.h\n", TEXT_MAX_LEN);
		goto err;
	}

	if (tr.io.input == INTERACTIVE)
		interactive_mode(&tr);
	else
		run(&tr);

	return EXIT_SUCCESS;

err:
	help(stderr);
	return EXIT_FAILURE;
}

