#include <stdio.h>
#include <stdlib.h>
#include "ca_module.h"


typedef struct {
    void *data;
} sample_ctx_t;


void *
module_init(ca_conf_ctx_t *conf)
{
    sample_ctx_t  *ctx = ca_alloc(sizeof(sample_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->data = (void *) "sample";

    return ctx;
}


int
module_process_task(void *context, void *task)
{
    printf("Hello world\n");
    ca_log_debug(0, "hello, world");
    return 0;
}


void
module_uninit(void *context)
{
    ca_free(context);
}


void
__sample_module_main(void)
{
    printf("*** [sample] module ***\n");
    printf("'sample' used as toturial for business development\n");
    printf("%s version: %s\n", PROG_NAME, CA_VERSION);
    exit(0);
}
