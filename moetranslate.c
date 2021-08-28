/* MIT License
 *
 * Copyright (c) 2021 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "lib/cJSON.h"
#include "lib/linenoise.h"
#include "lib/util.h"


/* macros */
enum InputMode {
	NORMAL = 1, INTERACTIVE
};

enum OutputMode {
	BRIEF = 1, DETAIL, RAW, DETECT_LANG
};

struct Lang {
	const char *const code, *const value;
};

struct Memory {
	char *value;
	size_t size;
};

struct Translate {
	char *text, *url;
	cJSON *result;

	struct {
		enum InputMode input;
		enum OutputMode output;
	} io;

	struct {
		const struct Lang *src, *target;
	} lang;
};


/* function declaration */
static void init(struct Translate *tr);
static void interactive_input(struct Translate *tr);
static void run(struct Translate *tr);
static const struct Lang *get_lang(const char *code);
static int set_lang(struct Translate *tr, char *codes);
static int url_parser(struct Translate *tr, const char *text_enc, size_t len);
static char *request_handler(struct Translate *tr);
static size_t write_callback(char *contents, size_t size, size_t nmemb, void *data);

static void brief_output(struct Translate *tr);
static void detail_output(struct Translate *tr);
static void raw_output(struct Translate *tr);
static void detail_output(struct Translate *tr);
static void detect_lang_output(struct Translate *tr);

static void help(void);


/* config.h for applying patches and the configurations */
#include "config.h"

/* color functions */
#define REGULAR_GREEN(TEXT)  "\033[00;" GREEN_COLOR  "m" TEXT "\033[00m"
#define REGULAR_YELLOW(TEXT) "\033[00;" YELLOW_COLOR "m" TEXT "\033[00m"

#define BOLD_BLUE(TEXT)      "\033[01;" BLUE_COLOR   "m" TEXT "\033[00m"
#define BOLD_GREEN(TEXT)     "\033[01;" GREEN_COLOR  "m" TEXT "\033[00m"
#define BOLD_WHITE(TEXT)     "\033[01;" WHITE_COLOR  "m" TEXT "\033[00m"
#define BOLD_YELLOW(TEXT)    "\033[01;" YELLOW_COLOR "m" TEXT "\033[00m"

/* global variables */

/* function array */
static void (*const Func[])(struct Translate *) = {
	[BRIEF]       = brief_output,
	[DETAIL]      = detail_output,
	[RAW]         = raw_output,
	[DETECT_LANG] = detect_lang_output
};

static const char *const output_str[] = {
	[BRIEF]       = "Brief",
	[DETAIL]      = "Detail",
	[RAW]         = "Raw",
	[DETECT_LANG] = "Detect Language"
};


/* function implementations */
static void
interactive_input(struct Translate *tr)
{
#define PRINT_INFO()\
	printf(BOLD_WHITE("----[ Moetranslate ]----")      \
	        "\n"                                       \
	        BOLD_YELLOW("Interactive input mode")      \
	        "\n\n"                                     \
	        BOLD_WHITE("Language    :") " [%s:%s]\n"   \
	        BOLD_WHITE("Output mode :") " %s\n"        \
		BOLD_WHITE("Show help   :") " Type /h\n\n" \
	        "------------------------\n",              \
	        tr->lang.src->code, tr->lang.target->code, \
	        output_str[tr->io.output])

#define PRINT_HELP()\
	printf("------------------------\n"                \
		BOLD_WHITE("Change Language:")             \
		"\n"                                       \
	        " /c [SOURCE]:[TARGET]\n\n"                \
	        BOLD_WHITE("Change Output Mode:")          \
		"\n"                                       \
	        " /o [OUTPUT]\n"                           \
	        "     OUTPUT:\n"                           \
                "      1 = Brief\n"                        \
	        "      2 = Detail\n"                       \
	        "      3 = Raw\n"                          \
	        "      4 = Detect Language\n\n"            \
	        BOLD_WHITE("Show Help:")                   \
		"\n"                                       \
	        " /h\n\n"                                  \
	        BOLD_WHITE("Quit:")                        \
		"\n"                                       \
	        " /q\n"                                    \
	        "------------------------\n")


	PRINT_INFO();

	char *result = NULL, *tmp;

	if (tr->text != NULL) {
		run(tr);
		puts("------------------------");
	}

	while (1) {
		FREE_N(result); /* see: lib/util.h */

		if ((result = linenoise(PROMPT)) == NULL)
			break;

		tmp = result;
		linenoiseHistoryAdd(result);

		if (strcmp(result, "/") == 0) {
			PRINT_INFO();
			continue;
		}
		if (strcmp(result, "/q") == 0)
			goto exit_l;
		if (strcmp(result, "/h") == 0) {
			PRINT_HELP();
			continue;
		}
		if (strncmp(result, "/c", 2) == 0) {
			if (set_lang(tr, tmp +2) < 0)
				goto err;
			
			printf("\nLanguage changed: "
				REGULAR_GREEN("[%s]") " -> "
				REGULAR_GREEN("[%s]") " \n\n",
				tr->lang.src->code, tr->lang.target->code);

			continue;
		}
		if (strncmp(result, "/o", 2) == 0) {
			enum OutputMode m = atoi(result+2);
			if (m == 0 || m >= LENGTH(output_str))
				goto err;

			tr->io.output = m;
			printf("\nMode output changed: "
				REGULAR_YELLOW("%s")
				"\n\n",
				output_str[m]);

			continue;
		}

		/* let's go! */
		if (strlen((tr->text = rtrim(ltrim(tmp)))) > 0) {
			puts("------------------------");
			run(tr);
			puts("------------------------");
		}

		continue;

	err:
		errno = EINVAL;
		perror(NULL);
	}

exit_l:
	FREE_N(result);
}

static void
init(struct Translate *tr)
{
	tr->text        = NULL;
	tr->url         = NULL;
	tr->result      = NULL;
	tr->io.input    = NORMAL;

	if (out_default_mode == 0 || out_default_mode >= LENGTH(output_str)) {
		errno = EINVAL;
		DIE("config.h: Unknown default output mode");
	}

	tr->io.output = out_default_mode;

	if ((tr->lang.src = get_lang(default_lang[0])) == NULL) {
		DIE("config.h");
	}

	if (strcmp(default_lang[1], "auto") == 0) {
		errno = EINVAL;
		DIE("Target language cannot be \"auto\"\nconfig.h");
	}

	if ((tr->lang.target = get_lang(default_lang[1])) == NULL) {
		DIE("config.h");
	}
}

static void
run(struct Translate *tr)
{
	char *mem = request_handler(tr);

	if ((tr->result = cJSON_Parse(mem)) == NULL) {
		errno = EINVAL;
		DIE("run(): cJSON_Parse(): Parsing error!");
	}

	Func[tr->io.output](tr);

	free(mem);
	cJSON_Delete(tr->result);
}

static const struct Lang *
get_lang(const char *code)
{
	if (code == NULL)
		goto err;

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

static int
set_lang(struct Translate *tr, char *codes)
{
	char *src    = strtok(codes, ":");
	char *target = strtok(NULL,  ":");

	src    = rtrim(ltrim(src));
	target = rtrim(ltrim(target));

	if (src == NULL || target == NULL) {
		fputs("Invalid language format! ", stderr);
		goto err;
	}
	if (strcmp(target, "auto") == 0) {
		fputs("Target language cannot be \"auto\" ", stderr);
		goto err;
	}

	const struct Lang *src_l, *target_l;
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
url_parser(struct Translate *tr, const char *text_enc, size_t len)
{
	switch (tr->io.output) {
	case DETECT_LANG:
		return snprintf(tr->url, len, URL_DETECT_LANG, text_enc);
	case BRIEF:
		return snprintf(tr->url, len, URL_BRIEF,
				tr->lang.src->code, tr->lang.target->code,
				text_enc);
	case RAW:
		/* because raw and detail output has the same url */
		/* FALLTHROUGH */
	case DETAIL:
		return snprintf(tr->url, len, URL_DETAIL,
				tr->lang.src->code, tr->lang.target->code,
				tr->lang.target->code, text_enc);
	}

	return -1;
}

static char *
request_handler(struct Translate *tr)
{
	char text_enc[TEXT_MAX_LEN *3];
	char url[sizeof(text_enc) + sizeof(URL_DETAIL)]; /* longest url */
	struct Memory memory = {0};
	CURLcode ccode;
	CURL *curl;

	if (tr->text == NULL) {
		errno = EINVAL;
		DIE("request_handler(): text is NULL");
	}

	curl = curl_easy_init();
	if (curl == NULL)
		DIE("request_handler(): curl_easy_init()");

	url_encode(text_enc, (unsigned char *)tr->text, sizeof(text_enc));

	tr->url = url;
	if (url_parser(tr, text_enc, sizeof(url)) < 0)
		DIE("request_handler(): url_parser()");

	curl_easy_setopt(curl, CURLOPT_URL, (char *)url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (struct Memory *)&memory);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT);

	ccode = curl_easy_perform(curl);
	if (ccode != CURLE_OK)
		DIE_E("request_handler()", curl_easy_strerror(ccode));

	tr->url  = NULL;
	curl_easy_cleanup(curl);

	return memory.value;
}

/* https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html */
static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	char *ptr;
	struct Memory *m = (struct Memory *)data;
	size_t r_size    = (size * nmemb);

	ptr = realloc(m->value, m->size + r_size +1);
	if (ptr == NULL)
		DIE("write_callback(): realloc");

	memcpy(ptr + m->size, contents, r_size);
	m->value = ptr;
	m->size += r_size;
	m->value[m->size] = '\0';

	return r_size;
}

static void
brief_output(struct Translate *tr)
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
detail_output(struct Translate *tr)
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

	printf("\n------------------------");


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

	printf("\n------------------------");

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

	printf("\n------------------------\n");

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
raw_output(struct Translate *tr)
{
	char *out = cJSON_Print(tr->result);

	puts(out);

	free(out);
}

static void
detect_lang_output(struct Translate *tr)
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
help(void)
{
	printf("moetranslate - A simple language translator\n\n"
		"Usage: moetranslate [-i/-b/-f/-r/-d/-h] [SOURCE] [TARGET] [TEXT]\n"
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
		"   Interactive :  moetranslate -i\n"
		"                  moetranslate -i -f auto:en\n"
	);
}


int
main(int argc, char *argv[])
{
	int opt;
	struct Translate tr;
	void (*run_tr)(struct Translate *) = run;

	if (argc <= 1 || strcmp(argv[1], "-") == 0)
		goto err;

	init(&tr);

	/* EXPERIMENTAL */
	while ((opt = getopt(argc, argv, "b:f:d:r:ih")) != -1) {
		switch (opt) {
		case 'b':
			tr.io.output = BRIEF;
			tr.text      = rtrim(ltrim(argv[optind]));
			set_lang(&tr, argv[optind-1]);
			break;
		case 'f':
			tr.io.output = DETAIL;
			tr.text      = rtrim(ltrim(argv[optind]));
			set_lang(&tr, argv[optind-1]);
			break;
		case 'd':
			tr.io.output = DETECT_LANG;
			tr.text      = rtrim(ltrim(argv[optind-1]));
			break;
		case 'r':
			tr.io.output = RAW;
			tr.text      = rtrim(ltrim(argv[optind]));
			set_lang(&tr, argv[optind-1]);
			break;
		case 'i':
			tr.io.input = INTERACTIVE;
			run_tr      = interactive_input;
			break;
		case 'h':
			/* FALLTHROUGH */
		default:
			goto err;
		}
	}

	if (tr.io.input == NORMAL && tr.text == NULL)
		goto err;

	run_tr(&tr);

	return EXIT_SUCCESS;

err:
	errno = EINVAL;
	perror(NULL);
	help();
	return EXIT_FAILURE;
}

