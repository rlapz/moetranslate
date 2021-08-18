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

#define FREE_N(X) if (X != NULL) free(X)


char *ltrim      (const char *str);
char *rtrim      (char *str);
char *trim_tag   (char *dest);
char *url_encode (char *dest, const unsigned char *src, size_t len);

