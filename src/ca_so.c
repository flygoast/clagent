#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "ca_core.h"
#include "ca_log.h"
#include "ca_so.h"


#define DLFUNC_NO_ERROR(h, v, name) do { \
    *(void **)(v) = dlsym(h, name); \
    dlerror(); \
} while (0)


#define DLFUNC(h, v, name) do { \
    *(void **)(v) = dlsym(h, name); \
    if ((error = dlerror()) != NULL) { \
        ca_log_crit(0, "symbol %s not found: ", sym[i].sym_name, error); \
        dlclose(h); \
        h = NULL; \
        return rc; \
    } \
} while (0)


int
ca_load_so(void **phandle, ca_symbol_t *sym, const char *filename)
{
    char    *error;
    int     rc = -1;
    int     i = 0;

    *phandle = dlopen(filename, RTLD_LAZY);
    if ((error = dlerror()) != NULL) {
        ca_log_crit(0, "dlopen %s failed: %s", filename, error);
        return -1;
    }

    ca_log_debug(0, "so %s loaded", filename);

    while (sym[i].sym_name) {
        if (sym[i].no_error) {
            DLFUNC_NO_ERROR(*phandle, sym[i].sym_ptr, sym[i].sym_name);
        } else {
            DLFUNC(*phandle, sym[i].sym_ptr, sym[i].sym_name);
        }
        ++i;
    }

    rc = 0;
    return rc;
}


void
ca_unload_so(void **phandle)
{
    if (*phandle != NULL) {
        if (dlclose(*phandle) != 0) {
            ca_log_crit(0, "%s", dlerror());
        }
        *phandle = NULL;
    }
}
