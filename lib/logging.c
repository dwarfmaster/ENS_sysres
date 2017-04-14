
#include "logging.h"
#include <stdio.h>
#include <stdarg.h>

void log_string(const char* s) {
    fprintf(stderr, "%s\n", s);
}

void log_variadic(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

