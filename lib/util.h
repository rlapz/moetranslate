#define DIE(msg)                                     \
	do {                                         \
		perror(msg);                         \
		exit(EXIT_FAILURE);                  \
	} while (0)

#define DIE_E(msg, p)                                \
	do {                                         \
		fprintf(stderr, "%s: %s\n", msg, p); \
		exit(EXIT_FAILURE);                  \
	} while (0)

#define LENGTH(X) (sizeof(X) / sizeof(X[0]))


char *lskip         (const char *str);
char *rskip         (char *str);
char *skip_html_tags(char *dest, size_t size);
char *url_encode    (char *dest, const char *src, size_t len);

