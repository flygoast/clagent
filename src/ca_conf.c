#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "clagent.h"


#define CA_CONF_BUFFER      4096


static ca_int_t ca_open_glob(ca_glob_t *gl);
static ca_int_t ca_read_glob(ca_glob_t *gl, ca_str_t *name);
static void ca_close_glob(ca_glob_t *gl);
static ca_int_t ca_conf_handler(ca_conf_t *cf, ca_int_t last);
static ca_int_t ca_conf_read_token(ca_conf_t *cf);
//static ca_int_t ca_conf_test_full_name(ca_str_t *name);
static char *ca_conf_include(ca_conf_t *cf, ca_command_t *cmd, void *conf);
static void ca_conf_free_args(ca_array_t *args);


static ca_uint_t argument_number[] = {
    CA_CONF_NOARGS,
    CA_CONF_TAKE1,
    CA_CONF_TAKE2,
    CA_CONF_TAKE3,
    CA_CONF_TAKE4,
    CA_CONF_TAKE5,
    CA_CONF_TAKE6,
    CA_CONF_TAKE7
};


static ca_command_t  ca_internal_commands[] = {

    { ca_string("include"),
      CA_CONF_TAKE1,
      ca_conf_include,
      0,
      0,
      NULL },

      ca_null_command
};


char *
ca_conf_param(ca_conf_t *cf, ca_str_t *param)
{
    char            *rv;
    ca_buf_t         b;
    ca_conf_file_t   conf_file;

    if (param->len == 0) {
        return CA_CONF_OK;
    }

    ca_memzero(&conf_file, sizeof(ca_conf_file_t));

    ca_memzero(&b, sizeof(ca_buf_t));

    b.start = param->data;
    b.pos = param->data;
    b.last = param->data + param->len;
    b.end = b.last;

    conf_file.file.fd = CA_INVALID_FILE;
    conf_file.file.name.data = NULL;
    conf_file.line = 0;

    cf->conf_file = &conf_file;
    cf->conf_file->buffer = &b;

    rv = ca_conf_parse(cf, NULL);

    cf->conf_file = NULL;

    return rv;
}


static ssize_t
ca_read_file(ca_file_t *file, u_char *buf, size_t size, off_t offset)
{
    ssize_t  n;

    if (file->sys_offset != offset) {
        if (lseek(file->fd, offset, SEEK_SET) != -1) {
            ca_log_crit(errno, "lseek() \"%s\" failed", file->name.data);
            return CA_ERROR;
        }

        file->sys_offset = offset;
    }

    n = read(file->fd, buf, size);

    if (n == -1) {
        ca_log_crit(errno, "read() \"%s\" failed", file->name.data);
        return CA_ERROR;
    }

    file->sys_offset += n;

    file->offset += n;

    return n;
}


char *
ca_conf_parse(ca_conf_t *cf, ca_str_t *filename)
{
    char             *rv;
    int               fd;
    ca_int_t          rc;
    ca_buf_t          buf;
    ca_conf_file_t   *prev, conf_file;
    ca_command_t     *ca_conf_commands;
    ca_array_t      **arg_array;
    enum {
        parse_file = 0,
        parse_block,
        parse_param
    } type;

    fd = CA_INVALID_FILE;
    prev = NULL;
    ca_conf_commands = cf->commands;

    if (ca_conf_commands == NULL) {
        ca_log_emerg(errno, "no command supplied");
        return CA_CONF_ERROR;
    }

    if (cf->args_array == NULL) {
        cf->args_array = ca_array_create(10, sizeof(ca_array_t *));
        if (cf->args_array == NULL) {
            return CA_CONF_ERROR;
        }
    }

    if (filename) {

        /* open configuration file */

        fd = open((const char *)filename->data, O_RDONLY, 0);
        if (fd == CA_INVALID_FILE) {
            ca_conf_log_error(CA_LOG_EMERG, cf, errno,
                              "open \"%s\" failed", filename->data);
            return CA_CONF_ERROR;
        }

        prev = cf->conf_file;

        ca_memzero(&conf_file, sizeof(ca_conf_file_t));

        cf->conf_file = &conf_file;
        if (fstat(fd, &cf->conf_file->file.info) == -1) {
            ca_log_emerg(errno, "stat \"%s\" failed", filename->data);
            goto failed;
        }

        cf->conf_file->buffer = &buf;

        buf.start = ca_alloc(CA_CONF_BUFFER);
        if (buf.start == NULL) {
            goto failed;
        }

        buf.pos = buf.start;
        buf.last = buf.start;
        buf.end = buf.last + CA_CONF_BUFFER;

        cf->conf_file->file.fd = fd;
        cf->conf_file->file.name.len = filename->len;
        cf->conf_file->file.name.data = filename->data;
        cf->conf_file->file.offset = 0;
        cf->conf_file->line = 1;

        type = parse_file;

    } else if (cf->conf_file->file.fd != CA_INVALID_FILE) {

        type = parse_block;

    } else {
        type = parse_param;
    }

    for ( ;; ) {

        rc = ca_conf_read_token(cf);

        /*
         * ca_conf_read_token() may return
         *
         *    CA_ERROR              there is error
         *    CA_OK                 the token terminated by ";" was found
         *    CA_CONF_BLOCK_START   the token terminated by "{" was found
         *    CA_CONF_BLOCK_DONE    the "}" was found
         *    CA_CONF_FILE_DONE     the configuration file is done
         */

        if (rc == CA_ERROR) {
            goto done;
        }

        if (rc == CA_CONF_BLOCK_DONE) {

            if (type != parse_block) {
                ca_conf_log_error(CA_LOG_EMERG, cf, 0, "unexpected \"}\"");
                goto failed;
            }

            goto done;
        }

        if (rc == CA_CONF_FILE_DONE) {

            if (type == parse_block) {
                ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                                  "unexpected end of file, expecting \"}\"");
                goto failed;
            }

            goto done;
        }

        if (rc == CA_CONF_BLOCK_START) {

            if (type == parse_param) {
                ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                                  "block directives are not supported "
                                  "in -g option");
                goto failed;
            }
        }

        /* rc == CA_OK || rc == CA_CONF_BLOCK_START */

        if (cf->handler) {

            /*
             * the custom handler
             */

            rv = (*cf->handler)(cf, NULL, cf->handler_conf);
            if (rv == CA_CONF_OK) {
                goto next;
            }

            if (rv == CA_CONF_ERROR) {
                goto failed;
            }

            ca_conf_log_error(CA_LOG_EMERG, cf, 0, rv);

            goto failed;
        }

        rc = ca_conf_handler(cf, rc);

        if (rc == CA_ERROR) {
            goto failed;
        }

next:

        arg_array = ca_array_push(cf->args_array);
        *arg_array = cf->args;
        cf->args = NULL;
    }

failed:

    rc = CA_ERROR;

done:

    if (cf->args) {
        arg_array = ca_array_push(cf->args_array);
        *arg_array = cf->args;
        cf->args = NULL;
    }

    if (filename) {
        if (cf->conf_file->buffer->start) {
            ca_free(cf->conf_file->buffer->start);
        }

        if (close(fd) == CA_FILE_ERROR) {
            ca_log_alert(errno, "close \"%s\" failed", filename->data);
            return CA_CONF_ERROR;
        }

        cf->conf_file = prev;
    }

    if (rc == CA_ERROR) {
        return CA_CONF_ERROR;
    }

    return CA_CONF_OK;
}


static ca_int_t
ca_conf_handler(ca_conf_t *cf, ca_int_t last)
{
    char          *rv;
    ca_str_t      *name;
    ca_command_t  *cmd;

    name = cf->args->elem;

    cmd = cf->commands;

    for ( /* void */; cmd->name.len; cmd++) {

        if (name->len != cmd->name.len) {
            continue;
        }

        if (ca_strcmp(name->data, cmd->name.data) != 0) {
            continue;
        }

        if (!(cmd->type & CA_CONF_BLOCK) && last != CA_OK) {
            ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                              "directive \"%s\" is not terminated by \";\"",
                              name->data);
            return CA_ERROR;
        }

        if ((cmd->type & CA_CONF_BLOCK) && last != CA_CONF_BLOCK_START) {
            ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                              "directive \"%s\" has no opening \"{\"",
                              name->data);
            return CA_OK;
        }

        /* is the directive's argument count right ? */

        if (!(cmd->type & CA_CONF_ANY)) {

            if (cmd->type & CA_CONF_FLAG) {

                if (cf->args->nelem != 2) {
                    goto invalid;
                }

            } else if (cmd->type & CA_CONF_1MORE) {

                if (cf->args->nelem < 2) {
                    goto invalid;
                }

            } else if (cmd->type & CA_CONF_2MORE) {

                if (cf->args->nelem < 3) {
                    goto invalid;
                }

            } else if (cf->args->nelem > CA_CONF_MAX_ARGS) {

                goto invalid;

            } else if (!(cmd->type & argument_number[cf->args->nelem - 1])) {

                goto invalid;
            }
        }

        rv = cmd->set(cf, cmd, cf->ctx);

        if (rv == CA_CONF_OK) {
            return CA_OK;
        }

        if (rv == CA_CONF_ERROR) {
            return CA_ERROR;
        }

        ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                          "\"%s\" directive %s", name->data, rv);

        return CA_ERROR;
    }

    cmd = ca_internal_commands;

    for ( /* void */; cmd->name.len; cmd++) {

        if (name->len != cmd->name.len) {
            continue;
        }

        if (ca_strcmp(name->data, cmd->name.data) != 0) {
            continue;
        }

        if (!(cmd->type & CA_CONF_BLOCK) && last != CA_OK) {
            ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                              "directive \"%s\" is not terminated by \";\"",
                              name->data);
            return CA_ERROR;
        }

        if ((cmd->type & CA_CONF_BLOCK) && last != CA_CONF_BLOCK_START) {
            ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                              "directive \"%s\" has no opening \"{\"",
                              name->data);
            return CA_OK;
        }

        /* is the directive's argument count right ? */

        if (!(cmd->type & CA_CONF_ANY)) {

            if (cmd->type & CA_CONF_FLAG) {

                if (cf->args->nelem != 2) {
                    goto invalid;
                }

            } else if (cmd->type & CA_CONF_1MORE) {

                if (cf->args->nelem < 2) {
                    goto invalid;
                }

            } else if (cmd->type & CA_CONF_2MORE) {

                if (cf->args->nelem < 3) {
                    goto invalid;
                }

            } else if (cf->args->nelem > CA_CONF_MAX_ARGS) {

                goto invalid;

            } else if (!(cmd->type & argument_number[cf->args->nelem - 1])) {

                goto invalid;
            }
        }

        rv = cmd->set(cf, cmd, cf->ctx);

        if (rv == CA_CONF_OK) {
            return CA_OK;
        }

        if (rv == CA_CONF_ERROR) {
            return CA_ERROR;
        }

        ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                          "\"%s\" directive %s", name->data, rv);

        return CA_ERROR;
    }

    ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                      "unknown directive \"%s\"", name->data);

    return CA_ERROR;

invalid:

    ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                      "invalid number of arguments in \"%s\" directive",
                      name->data);

    return CA_ERROR;
}


static ca_int_t
ca_conf_read_token(ca_conf_t *cf)
{
    u_char          *start, ch, *src, *dst;
    off_t            file_size;
    size_t           len;
    ssize_t          n, size;
    ca_uint_t        found, need_space, last_space, sharp_comment, variable;
    ca_uint_t        quoted, s_quoted, d_quoted, start_line;
    ca_str_t        *word;
    ca_buf_t        *b;

    found = 0;
    need_space = 0;
    last_space = 1;
    sharp_comment = 0;
    variable = 0;
    quoted = 0;
    s_quoted = 0;
    d_quoted = 0;

    if (cf->args != NULL) {
        ca_conf_free_args(cf->args);
        cf->args = NULL;
    }

    cf->args = ca_array_create(4, sizeof(ca_str_t));
    if (cf->args == NULL) {
        return CA_ERROR;
    }

    b = cf->conf_file->buffer;
    start = b->pos;
    start_line = cf->conf_file->line;

    file_size = ca_file_size(&cf->conf_file->file.info);

    for ( ;; ) {

        if (b->pos >= b->last) {

            if (cf->conf_file->file.offset >= file_size) {

                if (cf->args->nelem > 0 || !last_space) {

                    if (cf->conf_file->file.fd == CA_INVALID_FILE) {
                        ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                                          "unexpected end of parameter, "
                                          "expecting \";\"");
                        return CA_ERROR;
                    }

                    ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                                      "unexpected end of file, "
                                      "expecting \";\" or \"}\"");
                    return CA_ERROR;
                }

                return CA_CONF_FILE_DONE;
            }

            len = b->pos - start;

            if (len == CA_CONF_BUFFER) {
                cf->conf_file->line = start_line;

                if (d_quoted) {
                    ch = '"';

                } else if (s_quoted) {
                    ch = '\'';

                } else {
                    ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                                      "too long parameter \"%*s...\" started",
                                      10, start);
                    return CA_ERROR;
                }

                ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                                  "too long parameter, probably "
                                  "missing terminating \"%c\" character", ch);
                return CA_ERROR;
            }

            if (len) {
                ca_memmove(b->start, start, len);
            }

            size = (ssize_t) (file_size - cf->conf_file->file.offset);

            if (size > b->end - (b->start + len)) {
                size = b->end - (b->start + len);
            }

            n = ca_read_file(&cf->conf_file->file, b->start + len, size,
                             cf->conf_file->file.offset);

            if (n == CA_ERROR) {
                return CA_ERROR;
            }

            if (n != size) {
                ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                                  "read returned only %z bytes instead of %z",
                                  n, size);
                return CA_ERROR;
            }

            b->pos = b->start + len;
            b->last = b->pos + n;
            start = b->start;
        }

        ch = *b->pos++;

        if (ch == LF) {
            cf->conf_file->line++;

            if (sharp_comment) {
                sharp_comment = 0;
            }
        }

        if (sharp_comment) {
            continue;
        }

        if (quoted) {
            quoted = 0;
            continue;
        }

        if (need_space) {
            if (ch == ' ' || ch == '\t' || ch == CR || ch == LF) {
                last_space = 1;
                need_space = 0;
                continue;
            }

            if (ch == ';') {
                return CA_OK;
            }

            if (ch == '{') {
                return CA_CONF_BLOCK_START;
            }

            if (ch == ')') {
                last_space = 1;
                need_space = 0;

            } else {
                ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                                  "unexpected \"%c\"", ch);
                return CA_ERROR;
            }
        }

        if (last_space) {

            if (ch == ' ' || ch == '\t' || ch == CR || ch == LF) {
                continue;
            }

            start = b->pos - 1;
            start_line = cf->conf_file->line;

            switch (ch) {
            case ';':
            case '{':
                if (cf->args->nelem == 0) {
                    ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                                      "unexpected \"%c\"", ch);
                    return CA_ERROR;
                }

                if (ch == '{') {
                    return CA_CONF_BLOCK_START;
                }

                return CA_OK;

            case '}':
                if (cf->args->nelem != 0) {
                    ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                                      "unexpected \"}\"");
                    return CA_ERROR;
                }

                return CA_CONF_BLOCK_DONE;

            case '#':
                sharp_comment = 1;
                continue;

            case '\\':
                quoted = 1;
                last_space = 0;
                continue;
            
            case '"':
                start++;
                d_quoted = 1;
                last_space = 0;
                continue;

            case '\'':
                start++;
                s_quoted = 1;
                last_space = 0;
                continue;

            default:
                last_space = 0;
            }

        } else {

            if (ch == '{' && variable) {
                continue;
            }

            variable = 0;

            if (ch == '\\') {
                quoted = 1;
                continue;
            }

            if (ch == '$') {
                variable = 1;
                continue;
            }

            if (d_quoted) {
                if (ch == '"') {
                    d_quoted = 0;
                    need_space = 1;
                    found = 1;
                }

            } else if (s_quoted) {
                if (ch == '\'') {
                    s_quoted = 0;
                    need_space = 1;
                    found = 1;
                }

            } else if (ch == ' ' || ch == '\t' || ch == CR || ch == LF
                       || ch == ';' || ch == '{')
            {
                last_space = 1;
                found = 1;
            }

            if (found) {
                word = ca_array_push(cf->args);
                if (word == NULL) {
                    return CA_ERROR;
                }

                word->data = ca_alloc(b->pos - start + 1);
                if (word->data == NULL) {
                    return CA_ERROR;
                }

                for (dst = word->data, src = start, len = 0;
                     src < b->pos - 1;
                     len++)
                {
                    if (*src == '\\') {
                        switch (src[1]) {
                        case '"':
                        case '\'':
                        case '\\':
                            src++;
                            break;

                        case 't':
                            *dst++ = '\t';
                            src += 2;
                            continue;

                        case 'r':
                            *dst++ = '\r';
                            src += 2;
                            continue;

                        case 'n':
                            *dst++ = '\n';
                            src += 2;
                            continue;
                        }
                    }
                    *dst++ = *src++;
                }
                *dst = '\0';
                word->len = len;

                if (ch == ';') {
                    return CA_OK;
                }

                if (ch == '{') {
                    return CA_CONF_BLOCK_START;
                }

                found = 0;
            }
        }
    }
}


char *
ca_conf_include(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char        *rv;
    ca_int_t     n;
    ca_str_t    *value, file, name;
    ca_glob_t    gl;

    value = cf->args->elem;
    file = value[1];
    
    if (strpbrk((char *)file.data, "*?[") == NULL) {
        return ca_conf_parse(cf, &file);
    }

    ca_memzero(&gl, sizeof(ca_glob_t));
    gl.pattern = file.data;
    gl.test = 1;

    if (ca_open_glob(&gl) != CA_OK) {
        ca_conf_log_error(CA_LOG_EMERG, cf, errno,
                          "glob() \"%s\" failed", file.data);
        return CA_CONF_ERROR;
    }

    rv = CA_CONF_OK;

    for ( ;; ) {
        n = ca_read_glob(&gl, &name);

        if (n != CA_OK) {
            break;
        }

        file.len = name.len++;
        file.data = ca_alloc(file.len);
        if (file.data == NULL) {
            return CA_CONF_ERROR;
        }
        ca_cpystrn(file.data, name.data, file.len);

        rv = ca_conf_parse(cf, &file);

        ca_free(file.data);

        if (rv != CA_CONF_OK) {
            break;
        }
    }

    ca_close_glob(&gl);

    return rv;
}


void
ca_conf_log_error(ca_uint_t level, ca_conf_t *cf, int err, const char *fmt, ...)
{
    u_char   errstr[CA_MAX_CONF_ERRSTR], *p, *last;
    va_list  args;

    last = errstr + CA_MAX_CONF_ERRSTR;

    va_start(args, fmt);
    p = ca_vslprintf(errstr, last, fmt, args);
    va_end(args);

    if (err) {
        p = ca_log_errno(p, last, err);
    }

    if (cf->conf_file == NULL) {
        ca_log_core(level, 0, "%*s", p - errstr, errstr);
        return;
    }

    if (cf->conf_file->file.fd == CA_INVALID_FILE) {
        ca_log_core(level, 0, "%*s in command line",
                    p - errstr, errstr);
        return;
    }

    ca_log_core(level, 0, "%*s in %s:%ud",
                p - errstr, errstr,
                cf->conf_file->file.name.data, cf->conf_file->line);
}


static void
ca_conf_free_args(ca_array_t *args)
{
    ca_int_t   i;
    ca_str_t  *arg;

    if (args != NULL) {
        arg = args->elem;
        for (i = 0; i < args->nelem; i++) {
            ca_free(arg[i].data);
            arg[i].data = NULL;
            arg[i].len = 0;
        }

        ca_array_destroy(args);
    }
}


void
ca_conf_free(ca_conf_t *cf)
{
    ca_int_t      i;
    ca_array_t   *arg_array, **args_array;

    if (cf->args_array) {
        args_array = cf->args_array->elem;
        for (i = 0; i < cf->args_array->nelem; i++) {
            arg_array = args_array[i];
            ca_conf_free_args(arg_array);
        }
        ca_array_destroy(cf->args_array);
    }

    if (cf->args) {
        ca_conf_free_args(cf->args);
    }
}


char *
ca_conf_set_flag_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char            *p = conf;
    ca_str_t        *value;
    ca_flag_t       *fp;
    ca_conf_post_t  *post;

    fp = (ca_flag_t *)(p + cmd->offset);

    if (*fp != CA_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elem;

    if (ca_strcasecmp(value[1].data, (u_char *) "on") == 0) {
        *fp = 1;

    } else if (ca_strcasecmp(value[1].data, (u_char *) "off") == 0) {
        *fp = 0;

    } else {
        ca_conf_log_error(CA_LOG_EMERG, cf, 0, 
                          "invalid value \"%s\" in \"%s\" directive,"
                          "it must be \"on\" or \"off\"",
                          value[1].data, cmd->name.data);
        return CA_CONF_ERROR;
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, fp);
    }

    return CA_CONF_OK;
}


char *
ca_conf_set_str_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char            *p = conf;
    ca_str_t        *field, *value;
    ca_conf_post_t  *post;

    field = (ca_str_t *) (p + cmd->offset);

    if (field->data) {
        return "is duplicate";
    }

    value = cf->args->elem;

    *field = value[1];

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, field);
    }

    return CA_CONF_OK;
}


char *
ca_conf_set_str_array_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char             *p = conf;
    ca_str_t         *value, *s;
    ca_array_t      **a;
    ca_conf_post_t   *post;

    a = (ca_array_t **) (p + cmd->offset);

    if (*a == CA_CONF_UNSET_PTR) {
        *a = ca_array_create(4, sizeof(ca_str_t));
        if (*a == NULL) {
            return CA_CONF_ERROR;
        }
    }

    s = ca_array_push(*a);
    if (s == NULL) {
        return CA_CONF_ERROR;
    }

    value = cf->args->elem;

    *s = value[1];

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, a);
    }

    return CA_CONF_OK;
}


char *
ca_conf_set_keyval_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char             *p = conf;
    ca_str_t         *value;
    ca_array_t      **a;
    ca_keyval_t      *kv;
    ca_conf_post_t   *post;

    a = (ca_array_t **) (p + cmd->offset);

    if (*a == NULL) {
        *a = ca_array_create(4, sizeof(ca_keyval_t));
        if (*a == NULL) {
            return CA_CONF_ERROR;
        }
    }

    kv = ca_array_push(*a);
    if (kv == NULL) {
        return CA_CONF_ERROR;
    }

    value = cf->args->elem;

    kv->key = value[1];
    kv->value = value[2];
    
    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, kv);
    }

    return CA_CONF_OK;
}


char *
ca_conf_set_num_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char            *p = conf;
    ca_int_t        *np;
    ca_str_t        *value;
    ca_conf_post_t  *post;

    np = (ca_int_t *) (p + cmd->offset);

    if (*np != CA_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elem;

    *np = ca_atoi(value[1].data, value[1].len);
    if (*np == CA_ERROR) {
        return "invalid number";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, np);
    }

    return CA_CONF_OK;
}


char *
ca_conf_set_size_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char            *p = conf;
    size_t          *sp;
    ca_str_t        *value;
    ca_conf_post_t  *post;

    sp = (size_t *) (p + cmd->offset);

    if (*sp != CA_CONF_UNSET_SIZE) {
        return "is duplicate";
    }

    value = cf->args->elem;

    *sp = ca_parse_size(&value[1]);
    if (*sp == (size_t) CA_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, sp);
    }

    return CA_CONF_OK;
}


char *
ca_conf_set_msec_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char            *p = conf;
    ca_uint_t       *msp;
    ca_str_t        *value;
    ca_conf_post_t  *post;

    msp = (ca_uint_t *) (p + cmd->offset);
    if (*msp != CA_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elem;

    *msp = ca_parse_time(&value[1], 0);
    if (*msp == CA_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, msp);
    }

    return CA_CONF_OK;
}


char *
ca_conf_set_sec_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char            *p = conf;
    time_t          *sp;
    ca_str_t        *value;
    ca_conf_post_t  *post;

    sp = (time_t *) (p + cmd->offset);
    if (*sp != CA_CONF_UNSET) {
        return "is dupliate";
    }

    value = cf->args->elem;

    *sp = ca_parse_time(&value[1], 1);
    if (*sp == (time_t) CA_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, sp);
    }

    return CA_CONF_OK;
}


char *
ca_conf_set_enum_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char            *p = conf;
    ca_uint_t       *np, i;
    ca_str_t        *value;
    ca_conf_enum_t  *e;

    np = (ca_uint_t *) (p + cmd->offset);

    if (*np != CA_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elem;
    e = cmd->post;

    for (i = 0; e[i].name.len != 0; i++) {
        if (e[i].name.len != value[i].len
            || ca_strcasecmp(e[i].name.data, value[1].data) != 0)
        {
            continue;
        }

        *np = e[i].value;

        return CA_CONF_OK;
    }

    ca_conf_log_error(CA_LOG_WARN, cf, 0,
                      "invalid value \"%s\"", value[1].data);

    return CA_CONF_ERROR;
}


char *
ca_conf_set_bitmask_slot(ca_conf_t *cf, ca_command_t *cmd, void *conf)
{
    char               *p = conf;
    ca_uint_t          *np, i, m;
    ca_str_t           *value;
    ca_conf_bitmask_t  *mask;

    np = (ca_uint_t *) (p + cmd->offset);
    value = cf->args->elem;
    mask = cmd->post;

    for (i = 1; i < cf->args->nelem; i++) {
        for (m = 0; mask[m].name.len != 0; m++) {

            if (mask[m].name.len != value[i].len
                || ca_strcasecmp(mask[m].name.data, value[i].data) != 0)
            {
                continue;
            }

            if (*np & mask[m].mask) {
                ca_conf_log_error(CA_LOG_WARN, cf, 0,
                                  "duplicate value \"%s\"", value[i].data);

            } else {
                *np |= mask[m].mask;
            }

            break;
        }

        if (mask[m].name.len == 0) {
            ca_conf_log_error(CA_LOG_WARN, cf, 0,
                              "invalid value \"%s\"", value[i].data);

            return CA_CONF_ERROR;
        }
    }

    return CA_CONF_OK;
}


char *
ca_conf_check_num_bounds(ca_conf_t *cf, void *post, void *data)
{
    ca_conf_num_bounds_t  *bounds = post;
    ca_int_t              *np = data;

    if (bounds->high == -1) {
        if (*np >= bounds->low) {
            return CA_CONF_OK;
        }

        ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                          "value must be equal to or greater than %i",
                          bounds->low);

        return CA_CONF_ERROR;
    }

    if (*np >= bounds->low && *np <= bounds->high) {
        return CA_CONF_OK;
    }

    ca_conf_log_error(CA_LOG_EMERG, cf, 0,
                      "value must be between %i and %i",
                      bounds->low, bounds->high);

    return CA_CONF_ERROR;
}


static ca_int_t
ca_open_glob(ca_glob_t *gl)
{
    int  n;

    n = glob((char *) gl->pattern, 0, NULL, &gl->pglob);
    if (n == 0) {
        return CA_OK;
    }

#ifdef GLOB_NOMATCH

    if (n == GLOB_NOMATCH && gl->test) {
        return CA_OK;
    }

#endif

    return CA_ERROR;
}


static ca_int_t
ca_read_glob(ca_glob_t *gl, ca_str_t *name)
{
    size_t  count;

#ifdef GLOB_NOMATCH
    count = (size_t) gl->pglob.gl_pathc;
#else
    count = (size_t) gl->pglob.gl_matchc;
#endif

    if (gl->n < count) {

        name->len = (size_t) ca_strlen(gl->pglob.gl_pathv[gl->n]);
        name->data = (u_char *) gl->pglob.gl_pathv[gl->n];
        gl->n++;

        return CA_OK;
    }

    return CA_DONE;
}


static void
ca_close_glob(ca_glob_t *gl)
{
    globfree(&gl->pglob);
}
