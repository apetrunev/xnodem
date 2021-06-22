#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <gtmxc_types.h>

#define BUF_LEN_MAX (1*1024*1024)
#define BUF_LEN	    (1024)

static void gtm_error_parse(gtm_char_t *err_str, int *err_code, gtm_char_t **err_msg)
{
        gtm_char_t *code;
        /* parse gtm error string */
        code = strtok_r(err_str, ",", err_msg);
        *err_code = atoi(code);
}

int main(int argc, char **argv)
{
	gtm_status_t err;
	ci_name_descriptor call;
	gtm_char_t errbuf[BUF_LEN];
	gtm_char_t buf[BUF_LEN_MAX];
	char func[] = "set";

	call.rtn_name.address = func;
	call.rtn_name.length  = strlen(func);
	call.handle = NULL;

	char glb[]  = "dlw";
	char subs[] = "9:\"testing\",1:1";
	char data[] = "record 1";
	
	err = gtm_init();
	if (err)
		goto error;
	
	err = gtm_cip(&call, buf, glb, subs, data);
	if (err)
		goto error;

	err = gtm_exit();
	if (err)
		goto error;

	fprintf(stdout, "data: %s\n", buf);
	return EXIT_SUCCESS;			
error:
	gtm_zstatus(errbuf, sizeof(errbuf));
	fprintf(stderr, "error: %s\n", errbuf);
	return EXIT_FAILURE;
}

