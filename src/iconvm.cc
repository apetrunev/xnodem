/*
 * Copyright (C) JSC Komitex
 *
 * Alexey Petrunev petrunev_al@komitex.ru
 * 
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <assert.h>
#include <iconv.h>
#include <errno.h>

#ifdef __cplusplus
}
#endif

#include "iconvm.h"

static char stab[] = "__NOVALUE__";

iconv_t iconvm_open(char *to, char *from)
{
	assert(to != NULL);
	assert(from != NULL);
	
	return iconv_open(to, from);
}

size_t iconvm(iconv_t cd, char *in, size_t inlen, char *out, size_t outlen)
{
	char *ibuf, *obuf;
	size_t ileft, oleft, oleft_orig;
	size_t res;

	assert(in != NULL);
	assert(out != NULL);
	
	ileft = inlen;
	/* reserve space for null byte */
	oleft = oleft_orig = outlen - 1;		

	ibuf = in;
	obuf = out;
	
	while (ileft) {
		ileft = ileft > oleft ? oleft : ileft;
		res = iconv(cd, &ibuf, &ileft, &obuf, &oleft);
		if (res == (size_t)-1) {
			/* return converted data */
			if (oleft_orig - oleft > 0) {
				out[oleft_orig - oleft] = 0;
				return oleft_orig - oleft;
			}
			/* return stab */
			strncpy(out, stab, sizeof(stab));
			return strlen(stab);
		} 
	}
	/* terminate buffer with nul byte */	
	out[oleft_orig - oleft] = 0;
	/* return the size of converted data in the out buffer */
	return oleft_orig - oleft;
}

int iconvm_close(iconv_t cd)
{
	return iconv_close(cd);
}
