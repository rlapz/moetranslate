#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <json.h>


/* macros */
enum {
	BRIEF,
	FULL
};

typedef struct {
	char *src;
	char *dest;
	char *text;
} Lang;

typedef struct {
	char *memory;
	size_t size;
} Memory;

/* function declaration */
static void brief_mode(void);
static void full_mode(void);
static char *url_parser(CURL *curl, int mode);
static char *request_handler(CURL *curl, const char *url);
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *data);

/* variables */
static Lang lang;
static const char url_google[] = "https://translate.google.com/translate_a/single?";

static const char *url_params[] = {
	[BRIEF]	= "client=gtx&sl=%s&tl=%s&dt=t&q=%s",
	[FULL]	= "client=gtx&ie=UTF-8&oe=UTF-8&dt=bd&dt=x&dt=ld&dt=md&dt=rw&"
		  "dt=rm&dt=ss&dt=t&dt=at&dt=gt&dt=qc&sl=%s&tl=%s&hl=id&q=%s"
};


/* function implementations */
static void
brief_mode(void)
{
	char *url;
	char *dest;

	CURL *curl;
	url = url_parser(curl, BRIEF);

	/* init curl session */
	curl = curl_easy_init();
	if (!curl) {
		goto cleanup;
	}
	dest = request_handler(curl, url);
	if (!dest) {
		goto cleanup;
	}

	/* EXPERIMENTAL !! */
	/* JSON parser */

	json_object *jobj = json_tokener_parse(dest);
	json_object *jarray;
	int arraylen;

	arraylen = json_object_array_length(jobj);
	jarray = json_object_array_get_idx(jobj, 0);

	json_object *jvalue;
	json_object *ja;
	enum json_type type;
	for (int i = 0; i < arraylen; i++) {
		jvalue = json_object_array_get_idx(jarray, i);
		type = json_object_get_type(jvalue);
		if (type != json_type_object && type == json_type_array) {
			ja = json_object_array_get_idx(jvalue, 0);
			fprintf(stdout, "%s", json_object_get_string(ja));
		} else {
			puts("");
			break;
		}
	}

	while (json_object_put(jobj) != 1) {
		free(jobj);
	}

cleanup:
	if (dest)
		free(dest);
	if (url)
		free(url);
	if (curl)
		curl_easy_cleanup(curl);
}

static void
full_mode(void)
{
	char *url;
	char *dest;

	CURL *curl;
	url = url_parser(curl, FULL);

	/* init curl session */
	curl = curl_easy_init();
	if (!curl) {
		goto cleanup;
	}
	dest = request_handler(curl, url);
	if (!dest) {
		goto cleanup;
	}
	/* TODO */
	fprintf(stdout, dest);
cleanup:
	if (dest)
		free(dest);
	if (url)
		free(url);
	if (curl)
		curl_easy_cleanup(curl);
}

static char *
url_parser(CURL *curl, int mode)
{
	char *ret;
	char *tmp;
	char *curl_escape;
	size_t len_ret;

	/* create duplicate variable url_google */
	ret = strndup(url_google, strlen(url_google));
	if (!ret) {
		fprintf(stderr, "Error allocating memory!");
		return NULL;
	}
	/* we want appending url_google with url_params */
	tmp = realloc(ret, strlen(ret)+strlen(url_params[mode])+1);
	if (!tmp) {
		fprintf(stderr, "Error reallocating memory!");
		free(ret);
		return NULL;
	}
	ret = tmp;

	strncat(ret, url_params[mode], strlen(url_params[mode]));

	/* convert TEXT to url escape */
	curl_escape = curl_easy_escape(curl, lang.text, strlen(lang.text));

	len_ret = strlen(ret);
	tmp = realloc(ret, len_ret +
			strlen(curl_escape) +
			strlen(lang.src) + strlen(lang.dest)+1);
	if (!tmp) {
		fprintf(stderr, "Error reallocating memory!");
		free(ret);
		curl_free(curl_escape);
		return NULL;
	}
	ret = tmp;
	tmp = strdup(ret);
	sprintf(ret, tmp, lang.src, lang.dest, curl_escape);

	free(tmp);
	curl_free(curl_escape);
	return ret;
}

static char *
request_handler(CURL *curl, const char *url)
{
	Memory mem;
	CURLcode ccode;

	mem.memory = malloc(1);
	mem.size = 0;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&mem);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); /* timeout 10s */

	ccode = curl_easy_perform(curl);
	if (ccode != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform(): %s\n",
				curl_easy_strerror(ccode));
		free(mem.memory);
		return NULL;
	}

	return mem.memory;
}

static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	Memory *mem = (Memory*)data;

	char *ptr = realloc(mem->memory, mem->size + realsize +1);
	if (!ptr) {
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
	const char help[] = "%s SOURCE TARGET [-b] TEXT\n"
				"Example:\n"
				"\t[BRIEF MODE]\n"
				"\t%s en id -b \"hello\"\n"
				"\t[FULL MODE]\n"
				"\t%s en id \"hello\"\n";
	if (argc < 4) {
		fprintf(stderr, help, argv[0], argv[0], argv[0]);
		return 1;
	}

	lang.src = argv[1];
	lang.dest = argv[2];

	if (strcmp(argv[3], "-b") == 0) {
		if (argv[4] == NULL || strlen(argv[4]) == 0) {
			fprintf(stderr, help, argv[0], argv[0], argv[0]);
			return 1;
		}
		lang.text = argv[4];
		brief_mode();
	} else {
		lang.text = argv[3];
		full_mode();
	}

	return 0;
}
