#ifndef __CA_SO_H_INCLUDED__
#define __CA_SO_H_INCLUDED__


typedef struct {
    char   *sym_name;
    void  **sym_ptr;
    int     no_error; /* If the no_error is 1, the symbol is optional. */
} ca_symbol_t;


int ca_load_so(void **phandle, ca_symbol_t *syms, const char *filename);
void ca_unload_so(void **phandle);


#endif /* __CA_SO_H_INCLUDED__ */
