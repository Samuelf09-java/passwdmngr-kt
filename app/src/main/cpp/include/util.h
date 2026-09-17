#pragma once

#include <stdio.h>
#include <stdbool.h>

extern const char PATH_SEPARATOR;

#define X(x) (void)(x) // suppress 'unused parameter' compiler warnings with void cast
#define UNIMPLEMENTED util_error("This function is currently unimplemented") // mark a function as unimplemented

#define is_digit(c)                                                                                                    \
    (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' ||       \
     c == '9')
#define is_hex(c)                                                                                                      \
    (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' ||       \
     c == '9' || c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f' || c == 'A' || c == 'B' ||       \
     c == 'C' || c == 'D' || c == 'E' || c == 'F')
#define is_alpha(c) ((c >= 0x41 && c <= 0x5a) || (c >= 0x61 && c <= 0x7a))
#define is_horizontal_space(c) (c == ' ' || c == '\t')

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

enum ErrorType { WARN_D, NONFATAL_D, FATAL_D };

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

typedef enum ErrorType ErrorType;
typedef enum LogLevel  LogLevel;

char *util_get_app_dir();
char *util_get_logfile();
char *util_get_prefs_file();
char *util_get_accounts_file();
int   dir_exists(const char *path);

void util_assert(int cond, char *fail_msg);

void util_log(LogLevel level, const char *fmt, ...);

bool util_check_ptr(void *ptr, const char *msg);

void  wipe_mem(void *mem, size_t bytes);
void *ec_malloc(size_t size);
void *ec_calloc(size_t nmeb, size_t size);
void *ec_realloc(void *ptr, size_t size);
int   count_substrings(const char *haystack, const char *needle);
char *trim(char *s);

// wrappers for fgets and fread to assert return values
#define ec_fgets(__s, __n, __stream)                                                                                   \
    do {                                                                                                               \
        util_assert(fgets(__s, __n, __stream) != NULL, "fgets failed");                                                \
    } while (0)

static inline void ec_fread(void *__restrict__ __ptr, size_t __size, size_t __n, FILE *__restrict__ __stream) {
    size_t n = fread(__ptr, __size, __n, __stream);
    if (n < (size_t)__n) {
        if (ferror(__stream))
            util_log(LOG_ERROR, "fread returned short: read error");
        else if (feof(__stream))
            util_log(LOG_DEBUG, "fread returned short: EOF");
        else
            util_log(LOG_DEBUG, "fread returned short: unknown cause");
    }
}