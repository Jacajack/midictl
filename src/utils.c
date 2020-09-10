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
