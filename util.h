#define DIE(msg)                    \
	do {                        \
		perror(msg);        \
		exit(EXIT_FAILURE); \
	} while (0);

#define DIE_E(msg, p)                                \
	do {                                         \
		fprintf(stderr, "%s: %s\n", msg, p); \
		exit(EXIT_FAILURE);                  \
	} while (0);

#define LENGTH(X) (sizeof(X) / sizeof(X[0]))


char *ltrim      (const char *str);
char *rtrim      (char *str);
void  trim_tag   (char *dest, char tag);
char *url_encode (char *dest, const unsigned char *src, size_t len);

