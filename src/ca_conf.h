#ifndef __CA_CONF_H_INCLUDED__
#define __CA_CONF_H_INCLUDED__


#include <glob.h>
#include <sys/stat.h>
#include "clagent.h"


/*
 *        AAAA  number of arguments
 *      FF      commmand flags
 *    TT        dummy, not used
 */

#define CA_CONF_NOARGS      0x00000001
#define CA_CONF_TAKE1       0x00000002
#define CA_CONF_TAKE2       0x00000004
#define CA_CONF_TAKE3       0x00000008
#define CA_CONF_TAKE4       0x00000010
#define CA_CONF_TAKE5       0x00000020
#define CA_CONF_TAKE6       0x00000040
#define CA_CONF_TAKE7       0x00000080

#define CA_CONF_MAX_ARGS    8

#define CA_CONF_TAKE12      (CA_CONF_TAKE1|CA_CONF_TAKE2)
#define CA_CONF_TAKE13      (CA_CONF_TAKE1|CA_CONF_TAKE3)

#define CA_CONF_TAKE23      (CA_CONF_TAKE2|CA_CONF_TAKE3)

#define CA_CONF_TAKE123     (CA_CONF_TAKE1|CA_CONF_TAKE2|CA_CONF_TAKE3)
#define CA_CONF_TAKE1234    (CA_CONF_TAKE1|CA_CONF_TAKE2|CA_CONF_TAKE3  \
                             |CA_CONF_TAKE4)

#define CA_CONF_ARGS_NUMBER 0x000000ff
#define CA_CONF_BLOCK       0x00000100
#define CA_CONF_FLAG        0x00000200
#define CA_CONF_ANY         0x00000400
#define CA_CONF_1MORE       0x00000800
#define CA_CONF_2MORE       0x00001000


#define CA_CONF_UNSET       -1
#define CA_CONF_UNSET_UINT  (uint64_t)-1
#define CA_CONF_UNSET_PTR   (void *) -1
#define CA_CONF_UNSET_SIZE  (size_t) -1


#define CA_CONF_OK          0
#define CA_CONF_ERROR       (void *)-1


#define CA_CONF_BLOCK_START 1
#define CA_CONF_BLOCK_DONE  2
#define CA_CONF_FILE_DONE   3


#define CA_MAX_CONF_ERRSTR  1024


typedef struct ca_command_s  ca_command_t;
typedef struct ca_conf_s  ca_conf_t;


typedef struct {
    size_t      n;
    glob_t      pglob;
    u_char     *pattern;
    ca_uint_t   test;
} ca_glob_t;


typedef struct {
    int          fd;
    ca_str_t     name;
    struct stat  info;
    off_t        offset;
    off_t        sys_offset;
} ca_file_t;


typedef struct {
    ca_file_t       file;
    ca_buf_t       *buffer;
    ca_uint_t       line;
} ca_conf_file_t;


struct ca_command_s {
    ca_str_t        name;
    ca_uint_t       type;
    char          *(*set)(ca_conf_t *cf, ca_command_t *cmd, void *conf);
    ca_uint_t       conf;   /* dummy */
    ca_uint_t       offset;
    void           *post;
};


#define ca_null_command  { ca_null_string, 0, NULL, 0, 0, NULL }


typedef char *(*ca_conf_handler_pt)(ca_conf_t *cf, ca_command_t *dummy,
    void *conf);


struct ca_conf_s {
    char                *name;
    ca_array_t          *args;
    ca_conf_file_t      *conf_file;
    void                *ctx;
    ca_command_t        *commands;
    ca_array_t          *args_array;
    ca_conf_handler_pt   handler;
    char                *handler_conf;
};


typedef char *(*ca_conf_post_handler_pt)(ca_conf_t *cf, void *data, void *conf);

typedef struct {
    ca_conf_post_handler_pt  post_handler;
} ca_conf_post_t;


typedef struct {
    ca_conf_post_handler_pt  post_handler;
    ca_int_t                 low;
    ca_int_t                 high;
} ca_conf_num_bounds_t;


typedef struct {
    ca_str_t                 name;
    ca_uint_t                value;
} ca_conf_enum_t;


#define CA_CONF_BITMASK_SET  1

typedef struct {
    ca_str_t                 name;
    ca_uint_t                mask;
} ca_conf_bitmask_t;


char *ca_conf_check_num_bounds(ca_conf_t *cf, void *post, void *data);


#define ca_conf_init_value(conf, default)               \
    if (conf == CA_CONF_UNSET) {                        \
        conf = default;                                 \
    }

#define ca_conf_init_ptr_value(conf, default)           \
    if (conf == CA_CONF_UNSET_PTR) {                    \
        conf = default;                                 \
    }

#define ca_conf_init_uint_value(conf, default)          \
    if (conf == CA_CONF_UNSET_UINT) {                   \
        conf = default;                                 \
    }

#define ca_conf_init_size_value(conf, default)          \
    if (conf == CA_CONF_UNSET_SIZE) {                   \
        conf = default;                                 \
    }


void ca_conf_log_error(ca_uint_t level, ca_conf_t *cf, int err, 
    const char *fmt, ...);
char *ca_conf_param(ca_conf_t *cf, ca_str_t *param);
char *ca_conf_parse(ca_conf_t *cf, ca_str_t *filename);
void ca_conf_free(ca_conf_t *cf);


char *ca_conf_set_flag_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf);
char *ca_conf_set_str_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf);
char *ca_conf_set_str_array_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf);
char *ca_conf_set_str_keyval_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf);
char *ca_conf_set_num_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf);
char *ca_conf_set_str_size_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf);
char *ca_conf_set_str_msec_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf);
char *ca_conf_set_str_sec_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf);
char *ca_conf_set_str_enum_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf);
char *ca_conf_set_str_bitmask_slot(ca_conf_t *cf, ca_command_t *cmd,
    void *conf);

char *ca_conf_check_num_bounds(ca_conf_t *cf, void *post, void *data);


#endif /* __CA_CONF_H_INCLUDED__ */
