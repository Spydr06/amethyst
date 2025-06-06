#include <libgen.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

char *dirname(char *s)
{
	size_t i;
	if (!s || !*s) return ".";
	i = strlen(s)-1;
	for (; s[i]=='/'; i--) if (!i) return "/";
	for (; s[i]!='/'; i--) if (!i) return ".";
	for (; s[i]=='/'; i--) if (!i) return "/";
	s[i+1] = 0;
	return s;
}

char *basename(char *s)
{
    assert(!"unimplemented");
}
