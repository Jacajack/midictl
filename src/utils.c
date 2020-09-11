#include "utils.h"
#include <ctype.h>

/**
	\returns whether the string is only whitespace
*/
int isempty(const char *s)
{
	while (*s)
		if (!isspace(*s++))
			return 0;
	return 1;
}

/**
	Removes newline characters from the end of a string
*/
void trim_newline(char *s)
{
	char *p = s;
	while (*p) p++;
	p -= 1;
	while (p >= s && (*p == '\n' || *p == '\r'))
		*p-- = 0;
}

/**
	Removes whitespace from the end of a string
*/
void trim_r_whitespace(char *s)
{
	char *p = s;
	while (*p) p++;
	p -= 1;
	while (p >= s && isspace(*p))
		*p-- = 0;
}