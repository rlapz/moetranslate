#define LENGTH(X)   (sizeof(X) / sizeof(X[0]))
#define SUBTITLE(X) (X[0] = toupper((unsigned char)X[0]))


char *cskip_l        (const char *dest);
char *cskip_r        (char *dest, size_t len);
char *cskip_rl       (char *dest, size_t len);
char *cskip_a        (char *dest);
char *cskip_html_tags(char *dest, size_t len);
char *url_encode     (char *dest, const char *src, size_t len);

