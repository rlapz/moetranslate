/* MIT License
 *
 * Copyright (c) 2021 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <editline/readline.h>

#include "lib/cJSON.h"
#include "lib/util.h"



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
	size_t size ;
	char   val[];
} Memory;

typedef struct {
	const char *const code ;
	const char *const value;
} Lang;

typedef struct MoeTr {
	bool        is_connected;
	ResultType  result_type ;
	OutputMode  output_mode ;
	int         sock_d      ;
	const char *text        ;
	const Lang *lang_src    ;
	const Lang *lang_trg    ;
	Memory     *memory      ;
	char       *result      ;
} MoeTr;


/* function declarations */
static void        die              (const char *msg);
static void        load_config_h    (MoeTr *moe);
static void        setup            (MoeTr *moe);
static void        cleanup          (MoeTr *moe);
static const Lang *get_lang         (const char *code);
static int         set_lang         (MoeTr *moe, const char *codes);
static int         inet_connect     (MoeTr *moe);
static int         request_handler  (MoeTr *moe);
static int         response_handler (MoeTr *moe);
static int         run              (MoeTr *moe);
static int         run_intrc        (MoeTr *moe); // Interactive input
static Intrc_cmd   intrc_parse_cmd  (MoeTr *moe, const char *cmd);
static int         raw              (MoeTr *moe);
static int         parse            (MoeTr *moe);
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
static char prompt_intrc[16u + sizeof(PROMPT_LABEL)];

static const char *const result_type_str[] = {
	[BRIEF]    = "Brief",
	[DETAIL]   = "Detail",
	[DET_LANG] = "Detect Language"
};

static const char *const output_mode_str[] = {
	[PARSE] = "Parse",
	[RAW]   = "Raw"
};

/* function array */
static int (*const run_func[])(MoeTr *) = {
	[PARSE] = parse,
	[RAW]   = raw
};

static void (*const parse_func[])(MoeTr *, cJSON *) = {
	[BRIEF]    = parse_brief,
	[DETAIL]   = parse_detail,
	[DET_LANG] = parse_detect_lang
};


#define SET_LANG_PROMPT(SRC, TRG)\
	snprintf(prompt_intrc, sizeof(prompt_intrc), \
		"[ %s:%s ]%s", SRC, TRG, PROMPT_LABEL  \
	)



/* function implementations */
static inline void
die(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}


static void
load_config_h(MoeTr *moe)
{
	const char *err_msg;


	if (default_output_mode < PARSE || default_output_mode > RAW) {
		err_msg = "setup(): config.h: default_output_mode";
		goto err;
	}

	if (default_result_type < BRIEF || default_result_type > DET_LANG ) {
		err_msg = "setup(): config.h: default_result_type";
		goto err;
	}

	if ((moe->lang_src = get_lang(default_lang_src)) == NULL) {
		err_msg = "setup(): config.h: default_lang_src";
		goto err;
	}

	if (strcmp(default_lang_trg, "auto") == 0) {
		err_msg = "setup(): config.h: default_lang_trg cannot be \"auto\"";
		goto err;
	}

	if ((moe->lang_trg = get_lang(default_lang_trg)) == NULL) {
		err_msg = "setup(): config.h: default_lang_src";
		goto err;
	}

	moe->output_mode = default_output_mode;
	moe->result_type = default_result_type;

	return;

err:
	errno = EINVAL;
	die(err_msg);
}


static void
setup(MoeTr *moe)
{
	moe->memory = calloc(1u, sizeof(Memory) + BUFFER_SIZE);
	if (moe->memory == NULL)
		die("setup(): malloc");

	moe->memory->size = BUFFER_SIZE;
	moe->is_connected = false;
}


static void
cleanup(MoeTr *moe)
{
	if (moe->memory != NULL)
		free(moe->memory);

	if (moe->is_connected)
		close(moe->sock_d);
}


static inline const Lang *
get_lang(const char *code)
{
	size_t i = LENGTH(lang);


	do {
		i--;
		if (strcasecmp(code, lang[i].code) == 0)
			return &lang[i];
	} while (i > 0);

	return NULL;
}


static int
set_lang(MoeTr      *moe,
	 const char *codes)
{
	char        tmp[16];
	char       *src, *trg;
	const Lang *l_src, *l_trg;
	size_t      len = strlen(codes);


	if (len >= sizeof(tmp))
		goto err0;

	memcpy(tmp, codes, len +1u);
	src = cskip_a(tmp);

	if ((trg = strchr(src, ':')) == NULL)
		goto err0;

	*(trg++) = '\0';


	if (*src != '\0' && strcmp(src, moe->lang_src->code) != 0) {
		if ((l_src = get_lang(src)) == NULL) {
			fprintf(stderr, "Unknown \"%s\" language code\n", src);
			goto err0;
		}

		moe->lang_src = l_src;
	}

	if (*trg != '\0' && strcmp(trg, moe->lang_trg->code) != 0) {
		if (strcmp(trg, "auto") == 0) {
			fprintf(stderr, "Target language cannot be \"auto\"\n");
			goto err0;
		}

		if ((l_trg = get_lang(trg)) == NULL) {
			fprintf(stderr, "Unknown \"%s\" language code\n", trg);
			goto err0;
		}

		moe->lang_trg = l_trg;
	}

	return 0;

err0:
	errno = EINVAL;
	return -1;
}


static int
inet_connect(MoeTr *moe)
{
	int fd, ret;
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *ai, *p = NULL;


	if ((ret = getaddrinfo(URL, PORT, &hints, &ai)) != 0) {
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
			fprintf(stderr, "Retrying...\n");
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

	moe->is_connected = true;
	moe->sock_d       = fd;

	return fd;
}


static int
request_handler(MoeTr *moe)
{
#define TEXT_ENC_LEN   (TEXT_MAX_LEN * 3)
#define BUFFER_REQ_LEN (sizeof(HTTP_REQUEST_DETAIL) + TEXT_ENC_LEN)

	int     ret = 0;
	char    enc_text[TEXT_ENC_LEN];
	char    req_buff[BUFFER_REQ_LEN];

	size_t  req_len;
	size_t  b_total;
	ssize_t b_sent;


	url_encode(enc_text, moe->text, 0);

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

	default:
		errno = EINVAL;
		return -1;
	}

	if (ret < 0) {
		perror("request_handler(): snprintf");
		return -1;
	}

	req_len = (size_t)ret;
	b_total = 0;
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
	char    *p, *h_end;
	size_t   b_total = 0;
	ssize_t  b_recvd;


	do {
		if ((b_recvd = recv(moe->sock_d, &moe->memory->val[b_total],
				   moe->memory->size - b_total, 0)) < 0) {
			perror("response_handler(): recv");
			goto err;
		}

		b_total += (size_t)b_recvd;

		if (b_total == moe->memory->size) {
			const size_t new_size = moe->memory->size + b_recvd;
			Memory *new_mem = realloc(moe->memory,
						  sizeof(Memory) + new_size);

			if (new_mem == NULL) {
				perror("response_handler(): realloc");
				goto err;
			}

			new_mem->size = new_size;
			moe->memory   = new_mem;
		}

	} while (b_recvd > 0);

	moe->memory->val[b_total] = '\0';

	moe->result = moe->memory->val;

	if ((p = strstr(moe->result, "\r\n")) == NULL)
		goto err;

	h_end = p +2u;

	/* Check http response status */
	if ((p = strstr(moe->result, "200")) == NULL)
		goto err;

	/* Skipping \r\n\r\n */
	if ((p = strstr(h_end, "\r\n\r\n")) == NULL)
		goto err;

	if ((p = strstr(p +4u, "\r\n")) == NULL)
		goto err;


	/* Got the results */
	moe->result = p +2u;

	if ((p = strstr(moe->result, "\r\n")) == NULL)
		goto err;

	*p = '\0';

	return 0;

err:
	fprintf(stderr, "response_handler(): Failed to get the contents.\n");
	return -1;
}


static int
run(MoeTr *moe)
{
	int ret;

	setup(moe);

	if ((ret = inet_connect(moe)) < 0)
		goto cleanup;

	if ((ret = request_handler(moe)) < 0)
		goto cleanup;

	if ((ret = response_handler(moe)) < 0)
		goto cleanup;

	ret = run_func[moe->output_mode](moe);

cleanup:
	cleanup(moe);
	return ret;
}


static int
run_intrc(MoeTr *moe)
{
	Intrc_cmd prs;
	int       ret;
	char     *input, *tmp;

	info_intrc(moe);
	SET_LANG_PROMPT(moe->lang_src->code, moe->lang_trg->code);

	/* Show the results immediately if the text is not null */
	if (moe->text != NULL) {
		if ((ret = run(moe)) < 0)
			goto ret1;
		puts("------------------------\n");
	}

	ret = 0;
	while (1) {
		errno = 0;

		if ((input = readline(prompt_intrc)) == NULL) {
			putchar('\n');
			goto ret0;
		}

		add_history(input);

		tmp = cskip_rl(input, 0);
		prs = intrc_parse_cmd(moe, tmp);

		if (prs == QUIT)
			goto ret0;

		if (prs == OK || prs == ERR)
			goto free_res;

		/* let's go! */
		moe->text = tmp;
		if (*(moe->text) != '\0') {
			puts("------------------------\n");

			run(moe);

			puts("------------------------\n");
		}

	free_res:
		free(input);
	}

ret0:
	free(input);

ret1:
	return ret;
}


static Intrc_cmd
intrc_parse_cmd(MoeTr *moe,
		const char *cmd)
{
	ResultType res;
	OutputMode out;


	/* Summaries */
	if (*(cmd++) == '/') {
		switch (*cmd) {
		case '\0': goto info;
		case 'q' : { if (*(cmd +1u) == '\0') goto quit; } break;
		case 'h' : { if (*(cmd +1u) == '\0') goto help; } break;
		case 'c' : goto ch_lang;
		case 'o' : goto ch_output;
		case 'r' : goto ch_result;
		}

		goto err;
	}

	/* Default return */
	return MISS;


	/* Implementations */
info:
	info_intrc(moe);
	return OK;

quit:
	return QUIT;

help:
	help_intrc(moe);
	return OK;

ch_lang:
	if (set_lang(moe, cmd +1u) < 0)
		goto err;

	SET_LANG_PROMPT(moe->lang_src->code, moe->lang_trg->code);

	return OK;

ch_output:
	out = atoi(cmd +1u);

	switch (out) {
	case PARSE: moe->output_mode = PARSE; break;
	case RAW  : moe->output_mode = RAW  ; break;
	default   : goto err;
	}

	printf("\nMode output has been changed: "
		REGULAR_YELLOW("%s")
		"\n\n",
		output_mode_str[out]
	);
	return OK;

ch_result:
	res = atoi(cmd +1u);

	switch (res) {
	case BRIEF   : moe->result_type = BRIEF   ; break;
	case DETAIL  : moe->result_type = DETAIL  ; break;
	case DET_LANG: moe->result_type = DET_LANG; break;
	default      : goto err;
	}

	printf("\nResult type has been changed: "
		REGULAR_YELLOW("%s")
		"\n\n",
		result_type_str[res]
	);
	return OK;


err:
	errno = EINVAL;
	perror(NULL);
	putchar('\n');
	return ERR;
}


static int
raw(MoeTr *moe)
{
	return puts(moe->result);
}


static inline int
parse(MoeTr *moe)
{
	cJSON *json;

	if ((json = cJSON_Parse(moe->result)) == NULL)
		return -1;

	parse_func[moe->result_type](moe, json);

	cJSON_Delete(json);
	return 0;
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
	cJSON *def_cre;
	cJSON *def_vals;
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
		const Lang *src_l = get_lang(src_lang->valuestring);

		printf(REGULAR_GREEN("[ %s ]:") " %s\n\n",
			src_lang->valuestring,
			(src_l == NULL? "Unknown" : src_l->value)
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

	cJSON_ArrayForEach(i, synms) {
		int iter     = 1;
		int synn_max = SYNONYM_MAX_LINE;

		/* Verbs, Nouns, etc */
		if (*(i->child->valuestring) == '\0') {
			/* No label */
			printf("\n" BOLD_BLUE("[ + ]"));

		} else {
			SUBTITLE(i->child->valuestring);
			printf("\n" BOLD_BLUE("[ %s ]"), i->child->valuestring);
		}

		/* Target alternatives */
		cJSON_ArrayForEach(trg_synn, cJSON_GetArrayItem(i, 2)) {
			if (synn_max == 0)
				break;

			SUBTITLE(trg_synn->child->valuestring);
			printf("\n" BOLD_WHITE("%d. %s:") "\n   "
				REGULAR_YELLOW("-> "), iter, trg_synn->child->valuestring
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

	cJSON_ArrayForEach(i, defs) {
		int iter     = 1;
		int defs_max = DEFINITION_MAX_LINE;


		if (*(i->child->valuestring) == '\0') {
			/* No label */
			printf("\n" BOLD_YELLOW("[ + ]"));

		} else {
			SUBTITLE(i->child->valuestring);
			printf("\n" BOLD_YELLOW("[ %s ]"), i->child->valuestring);
		}

		cJSON_ArrayForEach(def_subs, cJSON_GetArrayItem(i, 1)) {
			if (defs_max == 0)
				break;

			SUBTITLE(def_subs->child->valuestring);
			printf("\n" BOLD_WHITE("%d. %s"),
				iter, def_subs->child->valuestring
			);

			def_cre = cJSON_GetArrayItem(def_subs, 3);
			if (cJSON_IsArray(def_cre) &&
					cJSON_IsString(def_cre->child->child)) {

				printf(REGULAR_GREEN(" [ %s ]"),
					def_cre->child->child->valuestring
				);
			}

			def_vals = cJSON_GetArrayItem(def_subs, 2);
			if (cJSON_IsString(def_vals)) {
				SUBTITLE(def_vals->valuestring);
				printf("\n" REGULAR_YELLOW("   ->") " %s ", def_vals->valuestring);
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

	cJSON_ArrayForEach(i, exmpls) {
		cJSON_ArrayForEach(exmpl_vals, i) {
			if (exmpls_max == 0)
				break;

			SUBTITLE(cskip_html_tags(exmpl_vals->child->valuestring, 0));
			printf("%d. " REGULAR_YELLOW("%s") "\n",
				iter, exmpl_vals->child->valuestring
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

	if (cJSON_IsString(lang_src)) {
		const Lang *src_l = get_lang(lang_src->valuestring);

		printf("%s (%s)\n",
			lang_src->valuestring,
			(src_l == NULL? "Unknown" : src_l->value)
		);
	}
}


static inline void
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


static inline void
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


static inline void
info_intrc(const MoeTr *moe)
{
	printf(BOLD_WHITE("----[ Moetranslate ]----")
		"\n"
	        BOLD_YELLOW("Interactive input mode")
	        "\n\n"
	        BOLD_WHITE("Languages   :")
		                        " %s (%s)\n"
		           "              %s (%s)\n"
	        BOLD_WHITE("Result type :") " %s\n"
	        BOLD_WHITE("Output mode :") " %s\n"
		BOLD_WHITE("Show help   :") " Type /h\n\n"
	        "------------------------\n",

	        moe->lang_src->value, moe->lang_src->code,
		moe->lang_trg->value, moe->lang_trg->code,
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

	setlocale(LC_CTYPE, "");
	load_config_h(&moe);

	while ((opt = getopt(argc, argv, "b:f:d:rih")) != -1) {
		switch (opt) {
		case 'b':
			moe.result_type = BRIEF;
			if (set_lang(&moe, cskip_l(argv[optind -1])) < 0)
				goto einv1;
			break;

		case 'f':
			moe.result_type = DETAIL;
			if (set_lang(&moe, cskip_l(argv[optind -1])) < 0)
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
		moe.text = cskip_rl(argv[optind -1], 0);
	else if (optind < argc)
		moe.text = cskip_rl(argv[optind], 0);
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

