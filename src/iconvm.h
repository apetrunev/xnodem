#ifndef ICONVM_H_
#define ICONVM_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <iconv.h>

iconv_t iconvm_open(char *to, char *from);
size_t iconvm(iconv_t cd, char *in, size_t inlen, char *out, size_t outlen);
int iconvm_close(iconv_t cd);

#ifdef __cplusplus
}
#endif

#endif /* ICONVM_H_ */
