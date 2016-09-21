#ifndef __CA_CORE_H_INCLUDED__
#define __CA_CORE_H_INCLUDED__


#include <stdint.h>
#include <sys/types.h>


#define LF              (u_char)10
#define CR              (u_char)13
#define CRLF            "\x0d\x0a"
#define CA_LINEFEED     "\x0a"


#define CA_OK            0
#define CA_ERROR        -1
#define CA_DONE         -2
#define CA_AGAIN        -3


typedef intptr_t        ca_int_t;
typedef uintptr_t       ca_uint_t;
typedef intptr_t        ca_flag_t;



#define CA_ABS(value)       (((value) >= 0) ? (value) : - (value))
#define CA_MAX(val1, val2)  ((val1 < val2) ? (val2) : (val1))
#define CA_MIN(val1, val2)  ((val1 > val2) ? (val2) : (val1))


#define CA_INT32_LEN   sizeof("-2147483648") - 1
#define CA_INT64_LEN   sizeof("-9223372036854775808") - 1


#if ((__GNU__ == 2) && (__GNUC_MINOR__ < 8))
#define CA_MAX_UINT32_VALUE  (uint32_t) 0xffffffffLL
#else
#define CA_MAX_UINT32_VALUE  (uint32_t) 0xffffffff
#endif

#define CA_MAX_INT32_VALUE   (uint32_t) 0x7fffffff

#define CA_MAX_INT64_VALUE   (uint64_t)0x7fffffffffffffffLL
#define CA_MAX_UINT64_VALUE  (uint64_t)0xffffffffffffffffLL

#define CA_INET_ADDRSTRLEN   (sizeof("255.255.255.255") - 1)
#define CA_SOCKADDR_STRLEN   (CA_INET_ADDRSTRLEN + sizeof(":65535") - 1)

#define CA_INVALID_FILE     -1
#define CA_INVALID_PID      -1
#define CA_FILE_ERROR       -1


#define ca_alloc        malloc
#define ca_calloc       calloc
#define ca_free         free
#define ca_realloc      realloc
#define ca_strdup       strdup


#define ca_file_size(sb)    (sb)->st_size


#ifdef WITH_DEBUG
#define ASSERT(x)           assert(x)  
#define SET_MAGIC(s, m)     (s)->magic = m
#else
#define ASSERT(x)           /* nothing */
#define SET_MAGIC(s, m)     /* nothing */
#endif


#endif /* __CA_CORE_H_INCLUDED__ */
