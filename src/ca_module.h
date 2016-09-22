#ifndef __CA_MODULE_H_INCLUDED__
#define __CA_MODULE_H_INCLUDED__


#include <sys/stat.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include "clagent.h"


__BEGIN_DECLS


void *module_init(ca_conf_ctx_t *conf);
int module_process_task(void *context, void *task);
void module_uninit(void *context);


/* Add interp section just for geek.
 * This partion of code can be remove to Makefile
 * to determine RTLD(runtime loader). */
#if __WORDSIZE == 64
#define RUNTIME_LINKER  "/lib64/ld-linux-x86-64.so.2"
#else
#define RUNTIME_LINKER  "/lib/ld-linux.so.2"
#endif

#ifndef __SO_INTERP__
#define __SO_INTERP__
const char __invoke_dynamic_linker__[] __attribute__ ((section (".interp")))
    = RUNTIME_LINKER;
#endif /* __SO_INTERP__ */


__END_DECLS


#endif /* __CA_MODULE_H_INCLUDED__ */
