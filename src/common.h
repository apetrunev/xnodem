#ifndef COMMON_H_
#define COMMON_H_

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif
#define BUF_LEN (4*1024) 
/* gtm related limits */
#define BUF_LEN_MAX (1*1024*1024)
#define SUBSCRIPT_LEN_MAX (32767)

#endif
