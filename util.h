#include <ctype.h>

#define LENGTH(X) (sizeof(X) / sizeof(X[0]))

void	 die		(const char *fmt, ...	);
char	*ltrim		(const char *str	);
char 	*rtrim		(char *str		);
void 	 trim_tag	(char *dest, char tag	);
char	*url_encode	(char *dest, const unsigned char *src, size_t len);

