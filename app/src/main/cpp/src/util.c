#include "../include/util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <android/log.h>

#ifdef _WIN32
const char PATH_SEPARATOR = '\\';
#else
const char PATH_SEPARATOR = '/';
#endif

char *app_dir = NULL;

char *util_get_logfile() {
    char *ret     = ec_malloc(strlen(app_dir) + strlen("passwdmngr.log") + 1);
    sprintf(ret, "%spasswdmngr.log", app_dir);
    return ret;
}

char *util_get_prefs_file() {
    char *ret     = ec_malloc(strlen(app_dir) + strlen("preferences.json") + 1);
    sprintf(ret, "%spreferences.json", app_dir);
    return ret;
}

char *util_get_accounts_file() {
    char *ret     = ec_malloc(strlen(app_dir) + strlen("accounts.bin") + 1);
    sprintf(ret, "%saccounts.bin", app_dir);
    return ret;
}

bool dir_exists(const char *path) {
    struct stat st;

    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode);

    return false;
}

bool delete_recursive(const char *path) {
    struct stat st;

    // If path doesn't exist, treat as success
    if (stat(path, &st) != 0) {
        return true;
    }

    // If it's a file, delete it
    if (!S_ISDIR(st.st_mode)) {
        return unlink(path) == 0;
    }

    // It's a directory — open it
    DIR *dir = opendir(path);
    if (!dir) {
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full child path
        char child_path[PATH_MAX];
        snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);

        // Recurse
        if (!delete_recursive(child_path)) {
            closedir(dir);
            return false;
        }
    }

    closedir(dir);

    // Delete the now-empty directory
    return rmdir(path) == 0;
}

void util_assert(int cond, char *fail_msg) {
    if (!cond) {
        util_log(LOG_FATAL, "Assertion failed: %s", fail_msg);
        exit(2); // failed assertion
    }
}

static char *vprintf_dup(const char *fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);

    int req_size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (req_size < 0) {
        return NULL;
    }

    char *buf = ec_malloc(req_size + 1);
    if (!buf) {
        return NULL;
    }

    vsnprintf(buf, req_size + 1, fmt, args);

    return buf;
}

void util_log(LogLevel level, const char *fmt, ...) {

    va_list args;
    va_start(args, fmt);

    char *msg = vprintf_dup(fmt, args);

    char *prefix = NULL;

    switch (level) {
        case LOG_DEBUG:
            prefix = "[passwdmngr/DEBUG]: ";
            __android_log_vprint(ANDROID_LOG_DEBUG, "passwdmngr", fmt, args);
            break;
        case LOG_INFO:
            prefix = "[passwdmngr/INFO]: ";
            __android_log_vprint(ANDROID_LOG_INFO, "passwdmngr", fmt, args);
            break;
        case LOG_WARN:
            prefix = "[passwdmngr/WARNING]: ";
            __android_log_vprint(ANDROID_LOG_WARN, "passwdmngr", fmt, args);
            break;
        case LOG_ERROR:
            prefix = "[passwdmngr/ERROR]: ";
            __android_log_vprint(ANDROID_LOG_ERROR, "passwdmngr", fmt, args);
            break;
        case LOG_FATAL:
            prefix = "[passwdmngr/FATAL ERROR]: ";
            __android_log_vprint(ANDROID_LOG_FATAL, "passwdmngr", fmt, args);
            break;
        default:
            prefix = "[passwdmgnr/UNKNOWN]: ";
            __android_log_vprint(ANDROID_LOG_UNKNOWN, "passwdmngr", fmt, args);
    }

    time_t     now      = time(NULL);
    struct tm *log_time = localtime(&now);
    char       time_buf[20];

    strftime(time_buf, sizeof(time_buf), "%m-%d-%Y %H:%M:%S", log_time);

    char *log_msg    = ec_malloc(strlen(msg) + strlen(prefix) + sizeof(time_buf) + 4);
    char *stdout_msg = ec_malloc(strlen(msg) + strlen(prefix) + 1);
    sprintf(log_msg, "(%s) %s%s", time_buf, prefix, msg);
    sprintf(stdout_msg, "%s%s", prefix, msg);
    free(msg);

    if (level > LOG_WARN)
        fprintf(stderr, "%s\n", stdout_msg);
    else
        printf("%s\n", stdout_msg);

    FILE *fp = fopen(util_get_logfile(), "a");
    if (fp) {
        fprintf(fp, "%s\n", log_msg);
        fclose(fp);
    }

    free(log_msg);
    free(stdout_msg);
    va_end(args);
}

bool util_check_ptr(void *ptr, const char *msg) {
    if (!ptr) {
        util_log(LOG_ERROR, msg);
        return false;
    }
    return true;
}

void wipe_mem(void *mem, size_t bytes) {
    memset(mem, 0, bytes);

#if defined(_MSC_VER)
    _ReadWriteBarrier();
#else
    __asm__ __volatile__("" : : "r"(mem) : "memory");
#endif
}

void *ec_malloc(size_t size) {
    void *ptr = malloc(size);
    util_assert(ptr != NULL, "malloc returned NULL pointer");
    return ptr;
}

void *ec_calloc(size_t nmeb, size_t size) {
    void *ptr = calloc(nmeb, size);
    util_assert(ptr != NULL, "calloc returned NULL pointer");
    return ptr;
}

void *ec_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    util_assert(new_ptr != NULL, "realloc returned NULL pointer");
    return new_ptr;
}

int count_substrings(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle)
        return 0;

    int    count = 0;
    size_t nlen  = strlen(needle);

    for (const char *h = haystack; *h; h++) {
        const char *h2 = h;
        const char *n2 = needle;

        while (*h2 && *n2 && tolower((unsigned char)*h2) == tolower((unsigned char)*n2)) {
            h2++;
            n2++;
        }

        if (!*n2) {
            count++;
            h += nlen - 1;
        }
    }

    return count;
}

char *trim(char *s) {
    char *start = s;
    char *end;

    while (is_horizontal_space((unsigned char)*start))
        start++;

    if (*start == '\0') {
        *s = '\0';
        return s;
    }

    end = start + strlen(start) - 1;
    while (end > start && is_horizontal_space((unsigned char)*end))
        end--;

    end[1] = '\0';
    memmove(s, start, end + 2 - start);

    return s;
}