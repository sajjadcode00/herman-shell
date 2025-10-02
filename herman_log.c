#include "herman_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#define LOG_FILE "/tmp/herman_debug.log"

static void write_log(const char *prefix, const char *format, va_list args) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) return;

    time_t t = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));

    fprintf(fp, "[%s] %s: ", ts, prefix);
    vfprintf(fp, format, args);
    fprintf(fp, "\n");
    fclose(fp);
}

void log_msg(const char *format, ...) {
    va_list args;
    va_start(args, format);
    write_log("INFO", format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    write_log("ERROR", format, args);
    va_end(args);
}
