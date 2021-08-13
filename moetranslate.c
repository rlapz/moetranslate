#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "cJSON.h"
#include "util.h"

typedef enum {
	NORMAL, INTERACTIVE, PIPE
} Input;

typedef enum {
	BRIEF, DETAIL, RAW, DETECT_LANG 
} Display;

struct Lang {
	char *code,
	     *value;
};

typedef struct {
	Display disp;
	Input   input;
	char *src, *target, *text;
	char *url;
	struct Lang *lang;
	cJSON *json;
} Translate;

typedef struct {
	char   *value;
	size_t  size;
} Memory;

/* function declaration, ordered in logical manner */
static void        interactive_mode(Translate *tr);
////static void        pipe_mode(Translate *tr);

static const char *get_lang(const char *lcode);
static void        run(Translate *tr);
static char       *url_parser(Translate *tr, size_t len);
static void        req_handler(Memory *mem, CURL *curl, const char *url);
static size_t      write_callback(char *p, size_t size, size_t nmemb, void *data);

static void        brief_output       (Translate *tr);
static void        detail_output      (Translate *tr);
static void        raw_output         (Translate *tr);
static void        detect_lang_output (Translate *tr);

static void        help(FILE *in);

#include "config.h"

static void
run(Translate *tr)
{	
	char url[(TEXT_MAX_LEN *3) + sizeof(URL_DETAIL)]; /* longest url */
	Memory mem = {0};
	CURL *curl;

	tr->url = &url[0];
	if (tr->src == NULL || tr->target == NULL)
		return;

	if (tr->input != INTERACTIVE && strlen(tr->text) >= TEXT_MAX_LEN)
		return;

	if (get_lang(tr->src) == NULL)
		return;
	if (strcmp(tr->target, "auto") == 0 || get_lang(tr->target) == NULL)
		return;

	curl    = curl_easy_init();
	tr->url = url_parser(tr, sizeof(url));

	req_handler(&mem, curl, tr->url);

	tr->json = cJSON_Parse(mem.value);

	switch (tr->disp) {
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

static const char *
get_lang(const char *lcode)
{
	size_t len = LENGTH(lang);

	for (size_t i = 0; i < len; i++) {
		if (strcmp(lcode, lang[i].code) == 0)
			return lang[i].value;
	}

	return NULL;
}

static char *
url_parser(Translate *tr, size_t len)
{
	int ret = -1;
	char text_enc[TEXT_MAX_LEN *3];

	url_encode(text_enc, (unsigned char *)tr->text, sizeof(text_enc));

	switch (tr->disp) {
	case DETECT_LANG:
		ret = snprintf(tr->url, len, URL_DETECT_LANG, text_enc);
		break;
	case BRIEF:
		ret = snprintf(tr->url, len, URL_BRIEF,
				tr->src, tr->target, text_enc);
		break;
	case RAW:
		/* because raw and full mode has the same url */
		/* FALLTHROUGH */
	case DETAIL:
		ret = snprintf(tr->url, len, URL_DETAIL,
				tr->src, tr->target, tr->target, text_enc);
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
	curl_easy_setopt(curl, CURLOPT_USERAGENT,     user_agent     );
	curl_easy_setopt(curl, CURLOPT_TIMEOUT,       timeout        );

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

	cJSON_ArrayForEach(i, tr->json->child) {
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
	cJSON *lang_src = tr->json->child->next->next;

	if (cJSON_IsString(lang_src)) {
		lang_c = lang_src->valuestring;
		printf("%s (%s)\n", lang_c, get_lang(lang_c));
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

	cJSON *result = tr->json;
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
raw_output(Translate *tr)
{
	char *out = cJSON_Print(tr->json);

	puts(out);

	free(out);
}

static void
interactive_mode(Translate *tr)
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

		buffer[strlen(buffer)-1] = '\0';
		tr->text = buffer;
		run(tr);
	}

	putchar('\n');
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
		"       -i         Interactive input\n"
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
	if (argc == 2 && strcmp(argv[1], "-h") == 0) {
		help(stdout);
		return 0;
	}

	Display disp;
	Input   input = NORMAL;

	if (argc == 3 && strcmp(argv[1], "-d") == 0) {
		disp = DETECT_LANG;
		goto run_tr;
	}

	if (argc != 4)
		return 1;

	if (strcmp(argv[1], "-i") == 0) {
		argv += 1;
		input = INTERACTIVE;
	}

	if (strcmp(argv[1], "-b") == 0)
		disp = BRIEF;
	else if (strcmp(argv[1], "-f") == 0)
		disp = DETAIL;
	else if (strcmp(argv[1], "-r") == 0)
		disp = RAW;
	else
		return 1;

	Translate tr = {
		.src    = strtok(argv[2], ":"),
		.target = strtok(NULL,    ":"),
		.text   = argv[3] ? rtrim(ltrim(argv[3])) : NULL,
		.input  = input,
		.disp   = disp,
	};

run_tr:
	if (input == INTERACTIVE)
		interactive_mode(&tr);
	else
		run(&tr);

	return 0;
}

