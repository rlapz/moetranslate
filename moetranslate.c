/* MIT License
 *
 * Copyright (c) 2021 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */
#include <stdio.h>
#include <string.h>
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

enum {
	AF=0, SQ, AM, AR, HY, AZ, EU, BE, BN, BS, BG, CA, CEB, ZHCN, ZHTW, CO,
	HR, CS, DA, NL, EN, EO, ET, FI, FR, FY, GL, KA, DE, EL, GU, HT, HA,
       	HAW, HE, HI, HMN, HU, IS, IG, ID, GA, IT, JA, JV, KN, KK, KM, RW, KO,
	KU, KY, LO, LA, LV, LT, LB, MK, MG, MS, ML, MI, MT, MR, MN, MY, NE, NO,
       	NY, OR, PS, FA, PL, PT, PA, RO, RU, SM, GD, SR, ST, SN, SD, SI, SK, SL,
       	SO, ES, SU, SW, SV, TL, TG, TA, TT, TE, TH, TR, TK, UK, UR, UG, UZ, VI,
       	CY, XH, YI, YO, ZU
};

struct Lang {
	int	mode;	/* mode translation */
	char	*src;	/* source language */
	char	*dest;	/* target language */
	char	*text;	/* text/words */
} __attribute__((__packed__));

struct Memory {
	char	*memory;
	size_t	size;
};

/* function declaration */
static char *get_lang(const char *lcode);
static void brief_mode(void);
static void full_mode(void);
static char *url_parser(CURL *curl);
static char *request_handler(void);
static char *html_cleaner(char **dest);
static char *string_append(char **dest, const char *fmt, ...);
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *data);

/* global variables */
static long timeout		= 10L; /* set request timout (10s) */
static struct Lang lang;
static const char url_google[]	= "https://translate.googleapis.com/translate_a/single?";
static const char *url_params[]	= {
	[BRIEF]	= "client=gtx&ie=UTF-8&oe=UTF-8&sl=%s&tl=%s&dt=t&q=%s",
	[FULL]	= "client=gtx&ie=UTF-8&oe=UTF-8&dt=bd&dt=ex&dt=ld&dt=md&dt=rw&"
		  "dt=rm&dt=ss&dt=t&dt=at&dt=gt&dt=qca&sl=%s&tl=%s&hl=id&q=%s"
};

/* 19 109 */
static const char *lang_code[109][17] = {
	[AF] = {"af", "Afrikaans"},
	[SQ] = {"sq", "Albanian"},
	[AM] = {"am", "Amharic"},
	[AR] = {"ar", "Arabic"},
	[HY] = {"hy", "Armenian"},
	[AZ] = {"az", "Azerbaijani"},
	[EU] = {"eu", "Basque"},
	[BE] = {"be", "Belarusian"},
	[BN] = {"bn", "Bengali"},
	[BS] = {"bs", "Bosnian"},
	[BG] = {"bg", "Bulgarian"},
	[CA] = {"ca", "Catalan"},
	[CEB] = {"ceb", "Cebuano"},
	[ZHCN] = {"zh-CN", "Chinese Simplified"},
	[ZHTW] = {"zh-TW", "Chinese Traditional"},
	[CO] = {"co", "Corsican"},
	[HR] = {"hr", "Croatian"},
	[CS] = {"cs", "Czech"},
	[DA] = {"da", "Danish"},
	[NL] = {"nl", "Dutch"},
	[EN] = {"en", "English"},
	[EO] = {"eo", "Esperanto"},
	[ET] = {"et", "Estonian"},
	[FI] = {"fi", "Finnish"},
	[FR] = {"fr", "French"},
	[FY] = {"fy", "Frisian"},
	[GL] = {"gl", "Galician"},
	[KA] = {"ka", "Georgian"},
	[DE] = {"de", "German"},
	[EL] = {"el", "Greek"},
	[GU] = {"gu", "Gujarati"},
	[HT] = {"ht", "Haitian Crole"},
	[HA] = {"ha", "Hausan"},
	[HAW] = {"haw", "Hawaiian"},
	[HE] = {"he", "Hebrew"},
	[HI] = {"hi", "Hindi"},
	[HMN] = {"hmn", "Hmong"},
	[HU] = {"hu", "Hungarian"},
	[IS] = {"is", "Icelandic"},
	[IG] = {"ig", "Igbo"},
	[ID] = {"id", "Indonesian"},
	[GA] = {"ga", "Irish"},
	[IT] = {"it", "Italian"},
	[JA] = {"ja", "Japanese"},
	[JV] = {"jv", "Javanese"},
	[KN] = {"kn", "Kannada"},
	[KK] = {"kk", "Kazakh"},
	[KM] = {"km", "Khmer"},
	[RW] = {"rw", "Kinyarwanda"},
	[KO] = {"ko", "Korean"},
	[KU] = {"ku", "Kurdish"},
	[KY] = {"ky", "Kyrgyz"},
	[LO] = {"lo", "Lao"},
	[LA] = {"la", "Latin"},
	[LV] = {"la", "Latvian"},
	[LT] = {"lt", "Lithunian"},
	[LB] = {"lb", "Luxembourgish"},
	[MK] = {"mk", "Macedonian"},
	[MG] = {"mg", "Malagasy"},
	[MS] = {"ms", "Malay"},
	[ML] = {"ml", "Malayam"},
	[MT] = {"mt", "Maltese"},
	[MI] = {"mi", "Maori"},
	[MR] = {"mr", "Marathi"},
	[MN] = {"mn", "Mongolian"},
	[MY] = {"my", "Myanmar"},
	[NE] = {"ne", "Nepali"},
	[NO] = {"no", "Norwebian"},
	[NY] = {"ny", "Nyanja"},
	[OR] = {"or", "Odia"},
	[PS] = {"ps", "Pashto"},
	[FA] = {"fa", "Persian"},
	[PL] = {"pl", "Polish"},
	[PT] = {"pt", "Portuguese"},
	[PA] = {"pa", "Punjabi"},
	[RO] = {"ro", "Romanian"},
	[RU] = {"ru", "Russian"},
	[SM] = {"sm", "Samoan"},
	[GD] = {"gd", "Scots Gaelic"},
	[SR] = {"sr", "Serbian"},
	[ST] = {"st", "Sesotho"},
	[SN] = {"sn", "Shona"},
	[SD] = {"sd", "Sindhi"},
	[SI] = {"si", "Sinhala"},
	[SK] = {"sk", "Slovak"},
	[SL] = {"sl", "Slovenian"},
	[SO] = {"so", "Somali"},
	[ES] = {"es", "Spanish"},
	[SU] = {"su", "Sundanese"},
	[SW] = {"sw", "Swahili"},
	[SV] = {"sv", "Swedish"},
	[TL] = {"tl", "Tagalog"},
	[TG] = {"tg", "Tajik"},
	[TA] = {"ta", "Tamil"},
	[TT] = {"tt", "Tatar"},
	[TE] = {"te", "Telugu"},
	[TH] = {"th", "Thai"},
	[TR] = {"tr", "Turkish"},
	[TK] = {"tk", "Turkmen"},
	[UK] = {"uk", "Ukranian"},
	[UR] = {"ur", "Urdu"},
	[UG] = {"ug", "Uyghur"},
	[UZ] = {"uz", "Uzbek"},
	[VI] = {"vi", "Vietnamese"},
	[CY] = {"cy", "Welsh"},
	[XH] = {"xh", "Xhosa"},
	[YI] = {"yi", "Yiddish"},
	[YO] = {"yo", "Yaruba"},
	[ZU] = {"zu", "Zulu"},
};

/* function implementations */
static char *
get_lang(const char *lcode)
{
	size_t i = 0;
	size_t len = strlen(lcode);
	for (; i < LENGTH(lang_code); i++) {
		if (strncmp(lcode, lang_code[i][0], len) == 0)
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
	char *result		= NULL;
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

	if (!(result = STRING_NEW()))
		goto cleanup;

	/* cJSON Parser */
	/* get translation */
	int count = 0;
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
		count++;
	}

	/* get spelling */
	spell = cJSON_GetArrayItem(cJSON_GetArrayItem(parser, 0), count -1);
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
		}
		string_append(&spell_str, "\n");
	}

	/* get correction */
	correct = cJSON_GetArrayItem(parser, 7);
	if (!(correct_str = STRING_NEW()))
		goto cleanup;
	if (cJSON_IsString(correct->child)) {
		string_append(&correct_str, "\nDid you mean: \"%s\"\n",
				correct->child->next->valuestring);
		memset(trans_src, 0, strlen(trans_src));
		string_append(&trans_src, correct->child->next->valuestring);
	}

	/* get language */
	langdest = cJSON_GetArrayItem(parser, 2);
	char *lang_v = NULL;
	if (!(lang_str = STRING_NEW()))
		goto cleanup;
	if (cJSON_IsString(langdest)) {
		lang_v = get_lang(langdest->valuestring);
		string_append(&lang_str, "\n[%s]: %s",
				langdest->valuestring,
				lang_v ? lang_v : ""
				);
	}

	/* get synonyms */
	char *syn_tmp;
	synonym = cJSON_GetArrayItem(parser, 1);
	if (!(syn_str = STRING_NEW()))
		goto cleanup;
	cJSON_ArrayForEach(iterator, synonym) {
		syn_tmp = iterator->child->valuestring;
		syn_tmp[0] = toupper(syn_tmp[0]);
		string_append(&syn_str, "\n\n\033[1m\033[37m[%s]:\033[0m",
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
	}

	/* get examples */
	int max = 5; /* examples max */
	example = cJSON_GetArrayItem(parser, 13);
	if (!(example_str = STRING_NEW()))
		goto cleanup;
	if (!cJSON_IsNull(example)) {
		string_append(&example_str, "\n\n%s\n",
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
	

	/* experimental */
	if (strlen(trans_src) < 1)
		goto cleanup;
	string_append(&result, "%s", correct_str);
	string_append(&result, "\"%s\"%s\n\n%s\n[%s]: %s\n",
			trans_src, lang_str, trans_dest,
			lang.dest, get_lang(lang.dest));
	string_append(&result, "%s", spell_str);
	string_append(&result, "%s", syn_str);
	string_append(&result, "%s", example_str);

	/* print to stdout */
	fprintf(stdout, "%s", result);

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
	if (result)
		free(result);
}

static char *
url_parser(CURL *curl)
{
	char *ret		= NULL;
	char *text_encode	= NULL;

	/* text encoding */
	text_encode = curl_easy_escape(curl, lang.text, (int)strlen(lang.text));
	if (!text_encode) {
		perror("url_parser(): curl_easy_escape()");
		return NULL;
	}

	if (!(ret = STRING_NEW()))
		goto cleanup;

	string_append(&ret, "%s", url_google);
	string_append(&ret, url_params[lang.mode],
			lang.src, lang.dest, text_encode);

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
	int i		= 0;
	
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

	lang.src = argv[1];
	lang.dest = argv[2];
	lang.mode = BRIEF;

	if (strncmp(lang.src, "auto", 5) != 0) {
		if (!get_lang(lang.src)) {
			fprintf(stderr, "Unknown \"%s\" language code\n", lang.src);
			return EXIT_FAILURE;
		}
	}
	if (!get_lang(lang.dest)) {
		fprintf(stderr, "Unknown \"%s\" language code\n", lang.dest);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[3], "-b") == 0) {
		if (argv[4] == NULL || strlen(argv[4]) == 0) {
			fprintf(stderr, help, argv[0], argv[0], argv[0]);
			return EXIT_FAILURE;
		}
		lang.text = argv[4];
	} else {
		lang.text = argv[3];
		lang.mode = FULL;
	}

	if (lang.mode == FULL)
		full_mode();
	else
		brief_mode();


	return EXIT_SUCCESS;
}

