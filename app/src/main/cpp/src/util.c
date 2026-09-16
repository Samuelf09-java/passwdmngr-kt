#include "../include/util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#ifdef _WIN32
const char PATH_SEPARATOR = '\\';
#else
const char PATH_SEPARATOR = '/';
#endif

char *util_get_app_dir() {
    char *home = getenv("HOME");
    char *root;

    if (!home)
        return NULL;
    root = ec_malloc(strlen(home) + strlen("/.local/share/passwdmngr/") + 1);
    if (!root) {
        util_log(LOG_ERROR, "Failed to allocate memory for root path");
        return NULL;
    }
    sprintf(root, "%s/.local/share/passwdmngr/", home);
    return root;
}

char *util_get_logfile() {
    char *basedir = util_get_app_dir();
    char *ret     = ec_malloc(strlen(basedir) + strlen("passwdmngr.log") + 1);
    sprintf(ret, "%spasswdmngr.log", basedir);
    free(basedir);
    return ret;
}

char *util_get_prefs_file() {
    char *basedir = util_get_app_dir();
    char *ret     = ec_malloc(strlen(basedir) + strlen("preferences.json") + 1);
    sprintf(ret, "%spreferences.json", basedir);
    free(basedir);
    return ret;
}

char *util_get_accounts_file() {
    char *basedir = util_get_app_dir();
    char *ret     = ec_malloc(strlen(basedir) + strlen("accounts.bin") + 1);
    sprintf(ret, "%saccounts.bin", basedir);
    free(basedir);
    return ret;
}

int dir_exists(const char *path) {
    return g_file_test(path, G_FILE_TEST_IS_DIR);
}

bool delete_recursive(const char *path, GError **error) {
    GFile *dir = g_file_new_for_path(path);

    // Check if directory exists
    if (!g_file_query_exists(dir, NULL)) {
        g_object_unref(dir);
        return true;
    }

    // Enumerate children
    GFileEnumerator *enumerator = g_file_enumerate_children(
        dir, G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_QUERY_INFO_NONE, NULL, error);

    if (!enumerator) {
        g_object_unref(dir);
        return false;
    }

    GFileInfo *info;
    while ((info = g_file_enumerator_next_file(enumerator, NULL, error))) {
        const char *name = g_file_info_get_name(info);
        GFileType   type = g_file_info_get_file_type(info);

        GFile *child = g_file_get_child(dir, name);

        if (type == G_FILE_TYPE_DIRECTORY) {
            // Recursively delete subdirectories
            if (!delete_recursive(g_file_get_path(child), error)) {
                g_object_unref(child);
                g_object_unref(info);
                g_object_unref(enumerator);
                g_object_unref(dir);
                return false;
            }
        }

        // Delete file or now-empty directory
        if (!g_file_delete(child, NULL, error)) {
            g_object_unref(child);
            g_object_unref(info);
            g_object_unref(enumerator);
            g_object_unref(dir);
            return false;
        }

        g_object_unref(child);
        g_object_unref(info);
    }

    g_object_unref(enumerator);

    // Delete the directory itself
    bool ok = g_file_delete(dir, NULL, error);
    g_object_unref(dir);
    return ok;
}

void util_assert(int cond, char *fail_msg) {
    if (!cond) {
        util_log(LOG_FATAL, "Assertion failed: %s", fail_msg);
        exit(2); // failed assertion
    }
}

void util_log(LogLevel level, const char *fmt, ...) {

#ifndef DEBUGMSG // set by Makefile to enable debugging
    if (level == LOG_DEBUG)
        return;
#endif

    va_list args;
    va_start(args, fmt);

    char *msg = g_strdup_vprintf(fmt, args);

    char *prefix = NULL;

    if (level == LOG_DEBUG)
        prefix = "[passwdmngr/DEBUG]: ";
    else if (level == LOG_INFO)
        prefix = "[passwdmngr/INFO]: ";
    else if (level == LOG_WARN)
        prefix = "[passwdmngr/WARNING]: ";
    else if (level == LOG_ERROR)
        prefix = "[passwdmngr/ERROR]: ";
    else
        prefix = "[passwdmngr/FATAL ERROR]: ";

    time_t     now      = time(NULL);
    struct tm *log_time = localtime(&now);
    char       time_buf[20];

    strftime(time_buf, sizeof(time_buf), "%m-%d-%Y %H:%M:%S", log_time);

    char *log_msg    = ec_malloc(strlen(msg) + strlen(prefix) + sizeof(time_buf) + 4);
    char *stdout_msg = ec_malloc(strlen(msg) + strlen(prefix) + 1);
    sprintf(log_msg, "(%s) %s%s", time_buf, prefix, msg);
    sprintf(stdout_msg, "%s%s", prefix, msg);
    g_free(msg);

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