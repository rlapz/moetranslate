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


char *cskip_l       (const char *dest);
char *cskip_r       (char *dest, size_t len);
char *cskip_rl      (char *dest, size_t len);
char *cskip_a       (char *dest);
char *skip_html_tags(char *dest, size_t len);
char *url_encode    (char *dest, const char *src, size_t len);

