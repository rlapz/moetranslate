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
	NORMAL = 1, INTERACTIVE
} InputMode;

typedef enum {
	BRIEF = 1, DETAIL, RAW, DETECT_LANG
} OutputMode;

typedef struct {
	char   *code, *value;
} Lang;

typedef struct {
	char   *url, *text;
	cJSON  *result;
	struct {
		OutputMode output;
		InputMode  input;
	} io;
	struct {
		const Lang *src, *target;
	} lang;
} Translate;

typedef struct {
	char   *value;
	size_t  size;
} Memory;


/* function declaration, ordered in logical manner */
static void        interactive_mode    (Translate *tr);
static void        run                 (Translate *tr);
static const Lang *get_lang            (const char *code);
static const char *get_output_mode_str (OutputMode out);
static int         set_lang            (Translate *tr, char *codes);

static int         url_parser          (Translate *tr, size_t len);
static void        req_handler         (Memory *dest, CURL *curl, const char *url);
static size_t      write_callback      (char *contents, size_t size, size_t nmemb,
                                        void *data);

static void        brief_output        (Translate *tr);
static void        detail_output       (Translate *tr);
static void        raw_output          (Translate *tr);
static void        detect_lang_output  (Translate *tr);
static void        help                (FILE *in);


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
#define PRINT_LANG_CH(L)\
	printf("\nLanguage changed: "             \
                REGULAR_GREEN("[%s]") " -> "   \
		REGULAR_GREEN("[%s]") " \n\n", \
		L.src->code, L.target->code)  \


#define PRINT_BANNER()\
	printf(BOLD_WHITE("----[ Moetranslate ]----")\
	        "\n"\
	        BOLD_YELLOW("Interactive input mode")\
	        "\n\n"\
	        "Lang        : [%s:%s]\n"\
	        "Output mode : %s\n"\
		"Show help   : type /h\n\n"\
	        "------------------------\n",\
	        tr->lang.src->code, tr->lang.target->code,\
	        get_output_mode_str(tr->io.output))

#define PRINT_HELP()\
	printf("Change Language:\n"\
	        " /c [SOURCE]:[TARGET]\n\n"\
	        "Change Output Mode:\n"\
	        " /o [OUTPUT]\n"\
	        "     OUTPUT:\n"\
                "      1 = Brief\n"\
	        "      2 = Detail\n"\
	        "      3 = Raw\n"\
	        "      4 = Detect Language\n\n"\
	        "Show Help:\n"\
	        " /h\n\n"\
	        "Quit:\n"\
	        " /q\n"\
	        "------------------------\n")\

	PRINT_BANNER();

	char *p          = NULL,
	     *tmp        = p;
	const char *cmd  = "Input text: ";

	while (1) {
		FREE_N(p); /* see: lib/util.h */

		if ((p = linenoise(cmd)) == NULL)
			break;

		tmp = p;
		linenoiseHistoryAdd(p);

		if (strcmp(p, "/q") == 0)
			goto exit_l;
		if (strcmp(p, "/h") == 0) {
			printf("\033[2J\033[H"); /* clear the screen */
			PRINT_HELP();
			continue;
		}
		if (strncmp(p, "/c", 2) == 0) {
			if (set_lang(tr, tmp +2) < 0)
				goto err;

			PRINT_LANG_CH(tr->lang);
			continue;
		}
		if (strncmp(p, "/o", 2) == 0) {
			OutputMode m = strtol(tmp +2, NULL, 10);
			if (get_output_mode_str(m) == NULL)
				goto err;

			tr->io.output = m;
			printf("\nMode output changed: %s\n\n",
					get_output_mode_str(m));

			continue;
		}

		/* let's go! */
		if (strlen((tr->text = rtrim(ltrim(tmp)))) > 0)
			run(tr);

		continue;

	err:
		errno = EINVAL;
		perror(NULL);
	}

exit_l:
	FREE_N(p);
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

	if ((tr->result = cJSON_Parse(mem.value)) == NULL) {
		errno = EINVAL;
		DIE("run(): cJSON_Parse(): Parsing error!");
	}

	switch (tr->io.output) {
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
	cJSON_Delete(tr->result);
	curl_easy_cleanup(curl);
}

static const Lang *
get_lang(const char *code)
{
	if (code == NULL) {
		fputs("Language code cannot be NULL\n", stderr);
		goto err;
	}

	size_t len = LENGTH(lang);

	for (size_t i = 0; i < len; i++) {
		if (strcmp(code, lang[i].code) == 0)
			return &lang[i];
	}

	fprintf(stderr, "Unknown \"%s\" language code\n", code);
err:
	errno = EINVAL;
	return NULL;
}

static const char *
get_output_mode_str(OutputMode out)
{
	switch (out) {
	case BRIEF:       return "Brief";
	case DETAIL:      return "Detail";
	case DETECT_LANG: return "Detect language";
	case RAW:         return "Raw";
	}

	return NULL;
}

static int
set_lang(Translate *tr, char *codes)
{
	char *src    = strtok(codes, ":");
	char *target = strtok(NULL,  ":");

	src    = rtrim(ltrim(src));
	target = rtrim(ltrim(target));

	if (src == NULL || target == NULL)
		goto err;
	if (strcmp(target, "auto") == 0) {
		fputs("Target language cannot be \"auto\"\n", stderr);
		goto err;
	}

	const Lang *src_l, *target_l; 
	if ((src_l = get_lang(src)) == NULL)
		goto err;
	if ((target_l = get_lang(target)) == NULL)
		goto err;

	tr->lang.src    = src_l;
	tr->lang.target = target_l;

	return 0;

err:
	errno = EINVAL;
	return -errno;
}

static int
url_parser(Translate *tr, size_t len)
{
	int ret = -1;
	char text_enc[TEXT_MAX_LEN *3];

	url_encode(text_enc, (unsigned char *)tr->text, sizeof(text_enc));

	switch (tr->io.output) {
	case DETECT_LANG:
		ret = snprintf(tr->url, len, URL_DETECT_LANG, text_enc);
		break;
	case BRIEF:
		ret = snprintf(tr->url, len, URL_BRIEF,
				tr->lang.src->code, tr->lang.target->code,
				text_enc);
		break;
	case RAW:
		/* because raw and detail output has the same url */
		/* FALLTHROUGH */
	case DETAIL:
		ret = snprintf(tr->url, len, URL_DETAIL,
				tr->lang.src->code, tr->lang.target->code,
				tr->lang.target->code, text_enc);
		break;
	};

	if (ret < 0) {
		errno = EINVAL;
		return -errno;
	}

	return 0;
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

	cJSON_ArrayForEach(i, (tr->result)->child) {
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
	cJSON *lang_src = (tr->result)->child->next->next;

	if (cJSON_IsString(lang_src)) {
		lang_c = lang_src->valuestring;
		printf("%s (%s)\n", lang_c, get_lang(lang_c)->value);
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

	cJSON *i;                            /* iterator      */
	cJSON *result         = tr->result;  /* temporary var */

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
			get_lang(src_lang->valuestring)->value);
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
			tr->lang.target->code, tr->lang.target->value);

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
	char *out = cJSON_Print(tr->result);

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
	Translate tr = {
		.io.output   = default_mode,
		.io.input    = NORMAL,
		.lang.src    = get_lang(default_lang[0]), /* set default lang */
		.lang.target = get_lang(default_lang[1])
	};

	/* dumb arg parser */
	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0) {
			help(stdout);
			return EXIT_SUCCESS;
		} else if (strcmp(argv[1], "-i") == 0) {
			tr.io.input = INTERACTIVE;
			goto run_tr;
		}
	}

	if (argc == 3 && strcmp(argv[1], "-d") == 0) {
		tr.io.output = DETECT_LANG;
		tr.text      = rtrim(ltrim(argv[2]));
		goto run_tr;
	}

	if (argc != 4)
		goto err;

	if (strcmp(argv[1], "-i") == 0) {
		argv += 1;
		tr.io.input = INTERACTIVE;
	}

	if (strcmp(argv[1], "-b") == 0)
		tr.io.output = BRIEF;
	else if (strcmp(argv[1], "-f") == 0)
		tr.io.output = DETAIL;
	else if (strcmp(argv[1], "-r") == 0)
		tr.io.output = RAW;
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
