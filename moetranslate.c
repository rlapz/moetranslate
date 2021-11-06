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

/* Command results for interactive input mode */
typedef enum {
	OK  ,
	MISS,
	EXIT,
	ERR ,
} Intrc_cmd;

typedef enum  {
	BRIEF   ,
	DETAIL  ,
	DET_LANG,
} ResultType;

typedef enum {
	PARSE,
	RAW  ,
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
static void        load_config_h    (MoeTr *m);
static void        setup            (MoeTr *m);
static void        cleanup          (MoeTr *m);
static const Lang *get_lang         (const char *code);
static int         set_lang         (MoeTr *m, const char *codes);
static int         inet_connect     (const char *addr, const char *port);
static int         request_handler  (MoeTr *m);
static int         response_handler (MoeTr *m);
static Memory     *resize_memory    (Memory *mem, size_t size);
static int         run              (MoeTr *m);
static int         run_intrc        (MoeTr *m); // Interactive input
static Intrc_cmd   intrc_parse_cmd  (MoeTr *m, const char *c);
static void        raw              (MoeTr *m);
static void        parse            (MoeTr *m);
static void        parse_brief      (MoeTr *m, cJSON *p);
static void        parse_detail     (MoeTr *m, cJSON *p);
static void        parse_detect_lang(MoeTr *m, cJSON *p);
static void        help             (void);
static void        help_intrc       (const MoeTr *m);
static void        info_intrc       (const MoeTr *m);


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
load_config_h(MoeTr *m)
{
	if (default_output_mode < 0 || default_output_mode > RAW) {
		errno = EINVAL;
		DIE("setup(): config.h: default_output_mode");
	}

	if (default_result_type <= 0 || default_result_type > DET_LANG ) {
		errno = EINVAL;
		DIE("setup(): config.h: default_result_type");
	}

	if (set_lang(m, default_langs) < 0)
		DIE("setup(): config.h: default_langs");

	m->output_mode = default_output_mode;
	m->result_type = default_result_type;
}


static void
setup(MoeTr *m)
{
	m->result = calloc(sizeof(Memory), sizeof(Memory));
	if (m->result == NULL)
		DIE("setup(): malloc");

	m->result->size = sizeof(char) * BUFFER_SIZE;
	m->result->val  = calloc(sizeof(char), m->result->size);

	if (m->result->val == NULL) {
		cleanup(m);

		DIE("setup(): malloc");
	}
}


static void
cleanup(MoeTr *m)
{
	if (m->result != NULL) {
		if (m->result->val != NULL) {
			m->result->size = 0;
			free(m->result->val);
		}

		free(m->result);
	}

	close(m->sock_d);
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
set_lang(MoeTr      *m,
	 const char *codes)
{
	const Lang *src_l, *trg_l;
	char       *src, *trg, *p;
	char        lcode[16];
	size_t      len_codes;

	if ((len_codes = strlen(codes) +1) >= sizeof(lcode))
		goto err;

	memcpy(lcode, codes, len_codes);

	if ((p = strstr(lcode, ":")) == NULL)
		goto err;

	*p = '\0';
	p++;

	src = RLSKIP(lcode);
	trg = RLSKIP(p);

	if (*src == '\0' || *trg == '\0')
		goto err;

	if (strcmp(trg, "auto") == 0)
		goto err;

	if ((src_l = get_lang(src)) == NULL)
		return -1;

	if ((trg_l = get_lang(trg)) == NULL)
		return -1;

	m->lang_src = src_l;
	m->lang_trg = trg_l;

	return 0;

err:
	errno = EINVAL;
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
request_handler(MoeTr *m)
{
#define TEXT_ENC_LEN   (TEXT_MAX_LEN * 3)
#define BUFFER_REQ_LEN (sizeof(HTTP_REQUEST_DETAIL) + TEXT_ENC_LEN)

	char    enc_text[TEXT_ENC_LEN];
	char    req_buff[BUFFER_REQ_LEN];
	int     ret = -1;

	size_t  req_len;
	size_t  text_len = strlen(m->text);
	size_t  b_total  = 0;
	ssize_t b_sent;


	url_encode(enc_text, m->text, text_len);

	switch (m->result_type) {
	case BRIEF:
		ret = snprintf(req_buff, BUFFER_REQ_LEN, HTTP_REQUEST_BRIEF,
				m->lang_src->code,
				m->lang_trg->code,
				enc_text);
		break;

	case DETAIL:
		ret = snprintf(req_buff, BUFFER_REQ_LEN, HTTP_REQUEST_DETAIL,
				m->lang_src->code,
				m->lang_trg->code,
				m->lang_trg->code,
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
		if ((b_sent = send(m->sock_d, &req_buff[b_total],
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
response_handler(MoeTr *m)
{
	char    *p, *h_end, *res;
	size_t   res_len;
	size_t   b_total = 0;
	ssize_t  b_sent;


	while (1) {
		if ((b_sent = recv(m->sock_d, &m->result->val[b_total],
				   m->result->size - b_total, 0)) < 0) {
			perror("response_handler(): recv");

			return -1;
		}

		if (b_sent == 0)
			break;

		b_total += (size_t)b_sent;

		if (b_total == m->result->size) {
			Memory       *new;
			const size_t  old_size = m->result->size;

			new = resize_memory(m->result,
					    old_size + (BUFFER_SIZE >> 1u));
			if (new == NULL) {
				perror("response_handler(): realloc");

				return -1;
			}

			m->result = new;
		}
	}

	m->result->val[b_total] = '\0';

	/* Get the contents (JSON) */
	if ((p = strstr(m->result->val, "\r\n")) == NULL)
		return -1;

	*p     = '\0';
	p     += 2;
	h_end  = p;
	p      = m->result->val;

	/* Check http response status */
	if ((p = strstr(p, "200")) == NULL)
		return -1;

	p = strstr(h_end, "\r\n\r\n");
	if (p == NULL)
		return -1;

	*p = '\0';

	/* Skipping \r\n\r\n */
	p += 4;

	if ((p = strstr(p, "\r\n")) == NULL)
		return -1;

	p      += 2;
	res     = p;
	res_len = strlen(p);

	if ((p = strstr(res, "\r\n")) == NULL)
		return -1;

	*p = '\0';

	memmove(m->result->val, res, res_len);
	m->result->val[res_len] = '\0';

	return 0;
}


static Memory *
resize_memory(Memory *mem, size_t size)
{
	char *p = realloc(mem->val, mem->size + size);
	if (p == NULL)
		return NULL;

	mem->val   = p;
	mem->size += size;

	return mem;
}


static int
run(MoeTr *m)
{
	setup(m);

	if ((m->sock_d = inet_connect(URL, "80")) < 0)
		return -1;

	if (request_handler(m) < 0)
		return -1;

	if (response_handler(m) < 0)
		return -1;


	run_func[m->output_mode](m);

	cleanup(m);

	return 0;
}


static int
run_intrc(MoeTr *m)
{
	Intrc_cmd prs;
	char *result = NULL, *tmp;

	info_intrc(m);

	/* Show the results immediately if the text is not null */
	if (m->text != NULL) {
		if (run(m) < 0)
			goto exit_l;
		puts("------------------------\n");
	}

	while (1) {
		if ((result = linenoise(PROMPT_LABEL)) == NULL)
			break;

		tmp = result;
		linenoiseHistoryAdd(result);

		prs = intrc_parse_cmd(m, tmp);

		if (prs == EXIT)
			goto exit_l;

		if (prs == OK || prs == ERR)
			goto free_res;

		/* let's go! */
		if (strlen((m->text = RLSKIP(tmp))) > 0) {
			puts("------------------------\n");
			if (run(m) < 0)
				goto exit_l;
			puts("------------------------\n");
		}

	free_res:
		free(result);
	}

exit_l:
	free(result);
	return 0;
}


static Intrc_cmd
intrc_parse_cmd(MoeTr *m, const char *c)
{
	if (strncmp(c, "/", 1) != 0)
		return MISS;

	if (strcmp(c, "/q") == 0) {
		return EXIT;

	} else if (strcmp(c, "/h") == 0) {
		help_intrc(m);

		return OK;

	} else if (strncmp(c, "/c", 2) == 0) {
		if (set_lang(m, c +2) < 0)
			goto err;

		printf("\nLanguage changed: "
			REGULAR_GREEN("[%s]") " -> "
			REGULAR_GREEN("[%s]") " \n\n",
			m->lang_src->code, m->lang_trg->code
		);

		return OK;

	} else if (strncmp(c, "/o", 2) == 0) {
		OutputMode d = atoi(c +2);

		switch (d) {
		case PARSE: m->output_mode = PARSE; break;
		case RAW:   m->output_mode = RAW  ; break;
		default:    goto err;
		}

		printf("\nMode output changed: "
			REGULAR_YELLOW("%s")
			"\n\n",
			output_mode_str[d]
		);

		return OK;

	} else if (strncmp(c, "/r", 2) == 0) {
		ResultType r = atoi(c +2);

		switch (r) {
		case BRIEF:    m->result_type = BRIEF   ; break;
		case DETAIL:   m->result_type = DETAIL  ; break;
		case DET_LANG: m->result_type = DET_LANG; break;
		default:       goto err;
		}

		printf("\nResult type changed: "
			REGULAR_YELLOW("%s")
			"\n\n",
			result_type_str[r]
		);

		return OK;
	}

err:
	errno = EINVAL;
	perror("> ");

	return ERR;
}


static void
raw(MoeTr *m)
{
	puts(m->result->val);
}


static void
parse(MoeTr *m)
{
	cJSON *p = cJSON_Parse(m->result->val);

	if (p == NULL)
		return;

	parse_func[m->result_type](m, p);

	cJSON_Delete(p);
}


static void
parse_brief(MoeTr *m, cJSON *p)
{
	(void)m;

	cJSON *i, *val;

	cJSON_ArrayForEach(i, p->child) {
		val = i->child;

		if (cJSON_IsString(val))
			printf("%s", val->valuestring);
	}

	putchar('\n');
}


static void
parse_detail(MoeTr *m, cJSON *p)
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

	cJSON *i;                   /* iterator      */
	cJSON *result         = p;  /* temporary var */

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
	printf("\"%s\"\n", m->text);

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
			m->lang_trg->code, m->lang_trg->value);

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
			skip_html_tags(expl_str);

			expl_str[0] = toupper(expl_str[0]);

			printf("%d. " REGULAR_YELLOW("%s") "\n", iter, expl_str);

			iter++;
			expl_max--;
		}
		putchar('\n');
	}
}


static void
parse_detect_lang(MoeTr *m, cJSON *p)
{
	(void)m;

	char  *lang_c;
	cJSON *lang_src = p->child->next->next;

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


static void
help_intrc(const MoeTr *m)
{
	printf("------------------------\n"
		BOLD_WHITE("Change the Languages:")
		" -> [%s:%s]\n"
	        " /c [SOURCE]:[TARGET]\n\n"
		BOLD_WHITE("Result Type:         ")
		" -> [%s]\n"
		" /r [TYPE]\n"
		"     TYPE:\n"
                "      0 = Brief\n"
	        "      1 = Detail\n"
	        "      2 = Detect Language\n\n"
	        BOLD_WHITE("Change Output Mode:  ")
		" -> [%s]\n"
	        " /o [OUTPUT]\n"
	        "     OUTPUT:\n"
		"      0 = Parse\n"
		"      1 = Raw\n\n"
	        BOLD_WHITE("Show Help:")
		"\n"
	        " /h\n\n"
	        BOLD_WHITE("Quit:")
		"\n"
	        " /q\n"
	        "------------------------\n",

		m->lang_src->code, m->lang_trg->code,
		result_type_str[m->result_type],
		output_mode_str[m->output_mode]
	);
}


static void
info_intrc(const MoeTr *m)
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

	        m->lang_src->code, m->lang_trg->code,
		result_type_str[m->result_type],
	        output_mode_str[m->output_mode]
	);
}



int
main(int argc, char *argv[])
{
	int   opt, ret = 0;
	bool  is_intrc = false, is_detc = false;
	MoeTr m = { 0 };


	if (argc == 1)
		goto einv0;

	load_config_h(&m);

	while ((opt = getopt(argc, argv, "b:f:d:rih")) != -1) {
		switch (opt) {
		case 'b':
			m.result_type = BRIEF;
			if (set_lang(&m, argv[optind -1]) < 0)
				goto einv1;

			break;

		case 'f':
			m.result_type = DETAIL;
			if (set_lang(&m, argv[optind -1]) < 0)
				goto einv1;

			break;

		case 'd':
			m.result_type = DET_LANG;
			is_detc       = true;
			break;

		case 'r':
			m.output_mode = RAW;
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
		m.text = RLSKIP(argv[optind -1]);

	else if (optind < argc)
		m.text = RLSKIP(argv[optind]);


	if (is_intrc)
		ret = run_intrc(&m);

	else 
		ret = run(&m);


	if (ret < 0)
		goto err;

	return EXIT_SUCCESS;

einv0:
	errno = EINVAL;

einv1:
	fprintf(stderr, "Moetranslate: %s!\n\n", strerror(errno));
	help();

err:
	return EXIT_FAILURE;
}

