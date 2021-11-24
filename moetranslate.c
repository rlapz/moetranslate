/* MIT License
 *
 * Copyright (c) 2021 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "lib/cJSON.h"
#include "lib/linenoise.h"
#include "lib/util.h"


#define RLSKIP(X) rskip(lskip(X))

/* Command's results in interactive input mode */
typedef enum {
	OK  = 1,
	MISS   ,
	QUIT   ,
	ERR    ,
} Intrc_cmd;

typedef enum  {
	BRIEF = 1,
	DETAIL   ,
	DET_LANG ,
} ResultType;

typedef enum {
	PARSE = 1,
	RAW      ,
} OutputMode;

typedef struct {
	char   *val ;
	size_t  size;
} Memory;

typedef struct {
	const char *const code ;
	const char *const value;
} Lang;

typedef struct MoeTr {
	ResultType  result_type;
	OutputMode  output_mode;
	int         sock_d     ;
	const char *text       ;
	char       *request    ;
	Memory     *result     ;
	const Lang *lang_src   ;
	const Lang *lang_trg   ;
} MoeTr;


/* function declarations */
static void        load_config_h    (MoeTr *moe);
static void        setup            (MoeTr *moe);
static void        cleanup          (MoeTr *moe);
static const Lang *get_lang         (const char *code);
static int         set_lang         (MoeTr *moe, const char *codes);
static int         inet_connect     (const char *addr, const char *port);
static int         request_handler  (MoeTr *moe);
static int         response_handler (MoeTr *moe);
static Memory     *resize_memory    (Memory **mem, size_t size);
static int         run              (MoeTr *moe);
static int         run_intrc        (MoeTr *moe); // Interactive input
static Intrc_cmd   intrc_parse_cmd  (MoeTr *moe, const char *cmd);
static void        raw              (MoeTr *moe);
static void        parse            (MoeTr *moe);
static void        parse_brief      (MoeTr *moe, cJSON *json);
static void        parse_detail     (MoeTr *moe, cJSON *json);
static void        parse_detect_lang(MoeTr *moe, cJSON *json);
static void        help             (void);
static void        help_intrc       (const MoeTr *moe);
static void        info_intrc       (const MoeTr *moe);


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
static const char *const result_type_str[] = {
	[BRIEF]    = "Brief",
	[DETAIL]   = "Detail",
	[DET_LANG] = "Detect Language"
};

static const char *const output_mode_str[] = {
	[PARSE] = "Parse",
	[RAW]   = "Raw"
};

static void (*const run_func[])(MoeTr *) = {
	[PARSE] = parse,
	[RAW]   = raw
};

static void (*const parse_func[])(MoeTr *, cJSON *) = {
	[BRIEF]    = parse_brief,
	[DETAIL]   = parse_detail,
	[DET_LANG] = parse_detect_lang
};


/* function implementations */
static void
load_config_h(MoeTr *moe)
{
	if (default_output_mode <= 0 || default_output_mode > RAW) {
		errno = EINVAL;
		DIE("setup(): config.h: default_output_mode");
	}

	if (default_result_type <= 0 || default_result_type > DET_LANG ) {
		errno = EINVAL;
		DIE("setup(): config.h: default_result_type");
	}

	if (set_lang(moe, default_langs) < 0)
		DIE("setup(): config.h: default_langs");

	moe->output_mode = default_output_mode;
	moe->result_type = default_result_type;
}


static void
setup(MoeTr *moe)
{
	moe->result = calloc(1u, sizeof(Memory) + BUFFER_SIZE);
	if (moe->result == NULL)
		DIE("setup(): malloc");

	moe->result->size = BUFFER_SIZE;
	moe->result->val  = (char *)moe->result + sizeof(Memory);
}


static void
cleanup(MoeTr *moe)
{
	if (moe->result != NULL)
		free(moe->result);

	close(moe->sock_d);
}


static const Lang *
get_lang(const char *code)
{
	size_t len_lang = LENGTH(lang);
	for (size_t i = 0; i < len_lang; i++) {
		if (strcmp(code, lang[i].code) == 0)
			return &lang[i];
	}

	errno = EINVAL;
	return NULL;
}


static int
set_lang(MoeTr      *moe,
	 const char *codes)
{
	const Lang *src_l, *trg_l;
	char       *src, *trg, *p;
	char        lcode[16];
	size_t      len_codes;

	if ((len_codes = strlen(codes) +1) >= sizeof(lcode))
		goto err0;

	memcpy(lcode, codes, len_codes);

	if ((p = strstr(lcode, ":")) == NULL)
		goto err0;

	*p = '\0';
	p++;

	src = RLSKIP(lcode);
	trg = RLSKIP(p);

	if (*src == '\0' || *trg == '\0')
		goto err0;

	if (strcmp(trg, "auto") == 0)
		goto err0;

	if ((src_l = get_lang(src)) == NULL)
		goto err1;

	if ((trg_l = get_lang(trg)) == NULL)
		goto err1;

	moe->lang_src = src_l;
	moe->lang_trg = trg_l;

	return 0;

err0:
	errno = EINVAL;

err1:
	return -1;
}


static int
inet_connect(const char *addr,
	     const char *port)
{
	int fd = 0, ret;
	struct addrinfo hints = { 0 }, *ai, *p = NULL;

	if ((ret = getaddrinfo(addr, port, &hints, &ai)) != 0) {
		fprintf(stderr, "inet_connect(): getaddrinfo: %s\n",
			gai_strerror(ret)
		);

		return -1;
	}

	for (p = ai; p != NULL; p = p->ai_next) {
		fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (fd < 0) {
			perror("inet_connect(): socket");

			continue;
		}

		if (connect(fd, p->ai_addr, p->ai_addrlen) < 0) {
			perror("inet_connect(): connect");
			close(fd);

			continue;
		}

		break;
	}

	freeaddrinfo(ai);

	if (p == NULL) {
		fprintf(stderr, "inet_connect(): Failed to connect\n");

		return -1;
	}

	return fd;
}


static int
request_handler(MoeTr *moe)
{
#define TEXT_ENC_LEN   (TEXT_MAX_LEN * 3)
#define BUFFER_REQ_LEN (sizeof(HTTP_REQUEST_DETAIL) + TEXT_ENC_LEN)

	char    enc_text[TEXT_ENC_LEN];
	char    req_buff[BUFFER_REQ_LEN];
	int     ret = -1;

	size_t  req_len;
	size_t  text_len = strlen(moe->text);
	size_t  b_total  = 0;
	ssize_t b_sent;


	url_encode(enc_text, moe->text, text_len);

	switch (moe->result_type) {
	case BRIEF:
		ret = snprintf(req_buff, BUFFER_REQ_LEN, HTTP_REQUEST_BRIEF,
				moe->lang_src->code,
				moe->lang_trg->code,
				enc_text);
		break;

	case DETAIL:
		ret = snprintf(req_buff, BUFFER_REQ_LEN, HTTP_REQUEST_DETAIL,
				moe->lang_src->code,
				moe->lang_trg->code,
				moe->lang_trg->code,
				enc_text);
		break;

	case DET_LANG:
		ret = snprintf(req_buff, BUFFER_REQ_LEN, HTTP_REQUEST_DET_LANG,
				enc_text);
		break;
	}

	if (ret < 0) {
		perror("request_handler(): snprintf");

		return -1;
	}

	req_len = strlen(req_buff);

	while (b_total < req_len) {
		if ((b_sent = send(moe->sock_d, &req_buff[b_total],
				   req_len - b_total, 0)) < 0) {
			perror("request_handler(): send");

			return -1;
		}

		if (b_sent == 0)
			break;

		b_total += (size_t)b_sent;
	}

	return 0;
}


static int
response_handler(MoeTr *moe)
{
	char    *p, *h_end, *res;
	size_t   res_len;
	size_t   b_total = 0;
	ssize_t  b_recvd;

	while (1) {

		if ((b_recvd = recv(moe->sock_d, &moe->result->val[b_total],
				   moe->result->size - b_total, 0)) < 0) {
			perror("response_handler(): recv");

			return -1;
		}

		if (b_recvd == 0)
			break;

		b_total += (size_t)b_recvd;

		if (b_total == moe->result->size) {
			Memory *new;

			new = resize_memory(&(moe->result),
						moe->result->size + b_recvd);

			if (new == NULL) {
				perror("response_handler(): realloc");

				return -1;
			}

			moe->result = new;
		}
	}

	moe->result->val[b_total] = '\0';

	if ((p = strstr(moe->result->val, "\r\n")) == NULL)
		goto err;

	*p     = '\0';
	p     += 2u;
	h_end  = p;
	p      = moe->result->val;

	/* Check http response status */
	if ((p = strstr(p, "200")) == NULL) {
		fprintf(stderr, "response_handler(): Response from the server: %s\n",
			moe->result->val
		);

		return -1;
	}

	p = strstr(h_end, "\r\n\r\n");
	if (p == NULL)
		goto err;

	*p = '\0';

	/* Skipping \r\n\r\n */
	p += 4u;

	if ((p = strstr(p, "\r\n")) == NULL)
		goto err;

	p      += 2u;
	res     = p;
	res_len = strlen(p);

	if ((p = strstr(res, "\r\n")) == NULL)
		goto err;

	*p = '\0';

	memmove(moe->result->val, res, res_len);

	return 0;

err:
	fprintf(stderr, "response_handler(): Failed to get the contents.\n");

	return -1;
}


static Memory *
resize_memory(Memory **mem, size_t size)
{
	Memory *new_mem;

	new_mem = realloc((*mem), sizeof(Memory) + (*mem)->size + size);
	if (new_mem == NULL)
		return NULL;

	new_mem->val   = (char *)new_mem + sizeof(Memory);
	new_mem->size += size;
	(*mem)         = new_mem;

	return *mem;
}


static int
run(MoeTr *moe)
{
	int ret = -1;

	setup(moe);

	if ((moe->sock_d = inet_connect(URL, PORT)) < 0)
		goto ret;

	if (request_handler(moe) < 0)
		goto cleanup;

	if (response_handler(moe) < 0)
		goto cleanup;

	run_func[moe->output_mode](moe);
	ret = 0;

cleanup:
	cleanup(moe);

ret:
	return ret;
}


static int
run_intrc(MoeTr *moe)
{
	int ret = 0;
	Intrc_cmd prs;
	char *input = NULL, *tmp;

	info_intrc(moe);

	/* Show the results immediately if the text is not null */
	if (moe->text != NULL) {
		if (run(moe) < 0)
			goto ret;
		puts("------------------------\n");
	}

	while (1) {
		errno = 0;

		if ((input = linenoise(PROMPT_LABEL)) == NULL)
			break;

		tmp = input;
		linenoiseHistoryAdd(input);

		prs = intrc_parse_cmd(moe, tmp);

		if (prs == QUIT)
			goto ret;

		if (prs == OK || prs == ERR)
			goto free_res;

		/* let's go! */
		if (strlen((moe->text = RLSKIP(tmp))) > 0) {
			puts("------------------------\n");

			if ((ret = run(moe)) < 0)
				goto ret;

			puts("------------------------\n");
		}

	free_res:
		free(input);
	}

ret:
	free(input);

	return ret;
}


static Intrc_cmd
intrc_parse_cmd(MoeTr *moe, const char *cmd)
{
	ResultType  r = 0;
	OutputMode  d = 0;
	const char *c = RLSKIP(cmd);

	if (strcmp(c, "/") == 0)
		goto info;

	if (strcmp(c, "/q") == 0)
		goto quit;

	if (strcmp(c, "/h") == 0)
		goto help;

	if (strncmp(c, "/c", 2) == 0)
		goto ch_lang;

	if (strncmp(c, "/o", 2) == 0)
		goto ch_output;

	if (strncmp(c, "/r", 2) == 0)
		goto ch_result;


	/* Default return */
	return MISS;

info:
	info_intrc(moe);

	return OK;

quit:
	return QUIT;

help:
	help_intrc(moe);

	return OK;

ch_lang:
	if (set_lang(moe, c +2) < 0)
		goto err;
	
	printf("\nLanguage changed: "
		REGULAR_GREEN("[%s]") " -> "
		REGULAR_GREEN("[%s]") " \n\n",
		moe->lang_src->code, moe->lang_trg->code
	);

	return OK;

ch_output:
	d = atoi(c +2);

	switch (d) {
	case PARSE: moe->output_mode = PARSE; break;
	case RAW:   moe->output_mode = RAW  ; break;
	default:    goto err;
	}

	printf("\nMode output changed: "
		REGULAR_YELLOW("%s")
		"\n\n",
		output_mode_str[d]
	);

	return OK;

ch_result:
	r = atoi(c +2);

	switch (r) {
	case BRIEF:    moe->result_type = BRIEF   ; break;
	case DETAIL:   moe->result_type = DETAIL  ; break;
	case DET_LANG: moe->result_type = DET_LANG; break;
	default:       goto err;
	}

	printf("\nResult type changed: "
		REGULAR_YELLOW("%s")
		"\n\n",
		result_type_str[r]
	);

	return OK;


err:
	errno = EINVAL;
	perror("Error");

	return ERR;
}


static void
raw(MoeTr *moe)
{
	puts(moe->result->val);
}


static void
parse(MoeTr *moe)
{
	cJSON *json;

	if ((json = cJSON_Parse(moe->result->val)) != NULL) {
		parse_func[moe->result_type](moe, json);

		cJSON_Delete(json);
	}
}


static void
parse_brief(MoeTr *moe, cJSON *json)
{
	(void)moe;

	cJSON *i;

	cJSON_ArrayForEach(i, json->child) {
		if (cJSON_IsString(i->child))
			printf("%s", i->child->valuestring);
	}

	putchar('\n');
}


static void
parse_detail(MoeTr *moe, cJSON *json)
{
	/*
	   source text 
	  	|
	   corrections
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

	cJSON *i;
	cJSON *tr_txt   = json->child;
	cJSON *exmpls   = cJSON_GetArrayItem(json, 13);
	cJSON *defs     = cJSON_GetArrayItem(json, 12);
	cJSON *splls    = cJSON_GetArrayItem(tr_txt, cJSON_GetArraySize(tr_txt) -1);
	cJSON *src_cor  = cJSON_GetArrayItem(json, 7);
	cJSON *src_lang = cJSON_GetArrayItem(json, 2);
	cJSON *synms    = cJSON_GetArrayItem(json, 1);
	cJSON *src_spll = cJSON_GetArrayItem(splls, 3);
	cJSON *trg_spll = cJSON_GetArrayItem(splls, 2);

	cJSON *src_synn;
	cJSON *trg_synn;
	cJSON *def_subs;
	cJSON *def_vals;
	cJSON *def_oths;
	cJSON *exmpl_vals;

	/* Source text */
	printf("\"%s\"\n", moe->text);

	/* Corrections */
	if (cJSON_IsString(src_cor->child)) {
		printf("\n"
			BOLD_YELLOW("Did you mean: ")
			"\"%s\" " BOLD_YELLOW("?") "\n\n",
			src_cor->child->next->valuestring
		);
	}

	/* Source spelling */
	if (cJSON_IsString(src_spll))
		printf("( " REGULAR_YELLOW("%s") " )\n", src_spll->valuestring);

	/* Source lang */
	if (cJSON_IsString(src_lang)) {
		printf(REGULAR_GREEN("[ %s ]:") " %s\n\n",
			src_lang->valuestring,
			get_lang(src_lang->valuestring)->value
		);
	}

	/* Target text */
	cJSON_ArrayForEach(i, tr_txt) {
		if (cJSON_IsString(i->child)) {
			printf(BOLD_WHITE("%s"), i->child->valuestring);
		}
	}

	putchar('\n');

	/* Target spelling */
	if (cJSON_IsString(trg_spll))
		printf("( " REGULAR_YELLOW("%s") " )\n", trg_spll->valuestring);

	/* Target lang */
	printf(REGULAR_GREEN("[ %s ]:") " %s\n",
		moe->lang_trg->code, moe->lang_trg->value
	);

	putchar('\n');

	/* Synonyms */
	if (!cJSON_IsArray(synms) || SYNONYM_MAX_LINE == 0)
		goto defs_sect;

	printf("\n------------------------");

	/* Labels */
	char *synn_lbl_str, *trg_synn_str;

	cJSON_ArrayForEach(i, synms) {
		int iter     = 1;
		int synn_max = SYNONYM_MAX_LINE;

		/* Verbs, Nouns, etc */
		synn_lbl_str    = i->child->valuestring;
		synn_lbl_str[0] = toupper(synn_lbl_str[0]);

		printf("\n" BOLD_BLUE("[ %s ]"), synn_lbl_str);

		/* Target alternatives */
		cJSON_ArrayForEach(trg_synn, cJSON_GetArrayItem(i, 2)) {
			if (synn_max == 0)
				break;

			trg_synn_str    = trg_synn->child->valuestring;
			trg_synn_str[0] = toupper(trg_synn_str[0]);

			printf("\n" BOLD_WHITE("%d. %s:") "\n  "
				REGULAR_YELLOW("-> "), iter, trg_synn_str
			);

			/* Source alternatives */
			int synn_src_size = cJSON_GetArraySize(
						cJSON_GetArrayItem(trg_synn, 1)) -1;

			cJSON_ArrayForEach(src_synn, cJSON_GetArrayItem(trg_synn, 1)) {
				printf("%s", src_synn->valuestring);

				if (synn_src_size > 0) {
					printf(", ");
					synn_src_size--;
				}
			}

			iter++;
			synn_max--;
		}
		putchar('\n');
	}
	putchar('\n');


defs_sect:
	/* Definitions */
	if (!cJSON_IsArray(defs) || DEFINITION_MAX_LINE == 0)
		goto exmpls_sect;

	printf("\n------------------------");

	char *def_lbl_str, *def_sub_str;

	cJSON_ArrayForEach(i, defs) {
		int iter     = 1;
		int defs_max = DEFINITION_MAX_LINE;

		def_lbl_str = i->child->valuestring;

		if (strlen(def_lbl_str) == 0)
			continue;

		def_lbl_str[0] = toupper(def_lbl_str[0]);

		printf("\n" BOLD_YELLOW("[ %s ]"), def_lbl_str);

		cJSON_ArrayForEach(def_subs, cJSON_GetArrayItem(i, 1)) {
			if (defs_max == 0)
				break;

			def_vals = cJSON_GetArrayItem(def_subs, 2);
			def_oths = cJSON_GetArrayItem(def_subs, 3);

			def_sub_str    = def_subs->child->valuestring;
			def_sub_str[0] = toupper(def_sub_str[0]);

			printf("\n" BOLD_WHITE("%d. %s") "\n  ", iter, def_sub_str);

			if (cJSON_IsString(def_vals))
				printf(REGULAR_YELLOW("->") " %s ", def_vals->valuestring);

			if (cJSON_IsArray(def_oths) &&
					cJSON_IsString(def_oths->child->child)) {

				printf(REGULAR_GREEN("[ %s ]"),
					def_oths->child->child->valuestring
				);
			}
			iter++;
			defs_max--;
		}
		putchar('\n');
	}
	putchar('\n');


exmpls_sect:
	if (!cJSON_IsArray(exmpls) || EXAMPLE_MAX_LINE == 0)
		return; /* It's over */

	printf("\n------------------------\n");

	int iter = 1, exmpls_max = EXAMPLE_MAX_LINE;
	char *exmpl_str;

	cJSON_ArrayForEach(i, exmpls) {
		cJSON_ArrayForEach(exmpl_vals, i) {
			if (exmpls_max == 0)
				break;

			exmpl_str = exmpl_vals->child->valuestring;
			exmpl_str[0] = toupper(skip_html_tags(exmpl_str, strlen(exmpl_str))[0]);

			printf("%d. " REGULAR_YELLOW("%s") "\n",
				iter, exmpl_str
			);

			iter++;
			exmpls_max--;
		}
		putchar('\n');
	}
}


static void
parse_detect_lang(MoeTr *moe, cJSON *json)
{
	(void)moe;

	cJSON *lang_src = json->child->next->next;

	if (cJSON_IsString(lang_src))
		printf("%s (%s)\n",
			lang_src->valuestring,
			get_lang(lang_src->valuestring)->value
		);
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


static void
help_intrc(const MoeTr *moe)
{
	printf("------------------------\n"
		BOLD_WHITE("Change the Languages:")
		" -> [%s:%s]\n"
	        " /c [SOURCE]:[TARGET]\n\n"
		BOLD_WHITE("Result Type:         ")
		" -> [%s]\n"
		" /r [TYPE]\n"
		"     TYPE:\n"
                "      1 = Brief\n"
	        "      2 = Detail\n"
	        "      3 = Detect Language\n\n"
	        BOLD_WHITE("Change Output Mode:  ")
		" -> [%s]\n"
	        " /o [OUTPUT]\n"
	        "     OUTPUT:\n"
		"      1 = Parse\n"
		"      2 = Raw\n\n"
	        BOLD_WHITE("Show Help:")
		"\n"
	        " /h\n\n"
	        BOLD_WHITE("Quit:")
		"\n"
	        " /q\n"
	        "------------------------\n",

		moe->lang_src->code, moe->lang_trg->code,
		result_type_str[moe->result_type],
		output_mode_str[moe->output_mode]
	);
}


static void
info_intrc(const MoeTr *moe)
{
	printf(BOLD_WHITE("----[ Moetranslate ]----")
	        "\n"
	        BOLD_YELLOW("Interactive input mode")
	        "\n\n"
	        BOLD_WHITE("Language    :") " [%s:%s]\n"
	        BOLD_WHITE("Result type :") " %s\n"
	        BOLD_WHITE("Output mode :") " %s\n"
		BOLD_WHITE("Show help   :") " Type /h\n\n"
	        "------------------------\n",

	        moe->lang_src->code, moe->lang_trg->code,
		result_type_str[moe->result_type],
	        output_mode_str[moe->output_mode]
	);
}



int
main(int argc, char *argv[])
{
	int   opt;
	bool  is_intrc = false;
	bool  is_detc  = false;
	MoeTr moe      = { 0 };


	if (argc == 1)
		goto einv0;

	load_config_h(&moe);

	while ((opt = getopt(argc, argv, "b:f:d:rih")) != -1) {
		switch (opt) {
		case 'b':
			moe.result_type = BRIEF;
			if (set_lang(&moe, argv[optind -1]) < 0)
				goto einv1;

			break;

		case 'f':
			moe.result_type = DETAIL;
			if (set_lang(&moe, argv[optind -1]) < 0)
				goto einv1;

			break;

		case 'd':
			moe.result_type = DET_LANG;
			is_detc         = true;

			break;

		case 'r':
			moe.output_mode = RAW;

			break;

		case 'i':
			is_intrc = true;

			break;

		case 'h':
			help();

			return EXIT_SUCCESS;

		default:
			goto einv0;
		}
	}


	if (is_detc)
		moe.text = RLSKIP(argv[optind -1]);

	else if (optind < argc)
		moe.text = RLSKIP(argv[optind]);

	else
		moe.text = NULL;


	if (is_intrc) {
		if (run_intrc(&moe) < 0)
			goto err;

	} else if (moe.text != NULL) {
		if (run(&moe) < 0)
			goto err;

	} else {
		goto einv0;
	}


	return EXIT_SUCCESS;

einv0:
	errno = EINVAL;

einv1:
	fprintf(stderr, "Error: %s!\n\n", strerror(errno));
	help();

err:
	return EXIT_FAILURE;
}

