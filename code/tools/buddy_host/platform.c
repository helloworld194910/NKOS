/* Compile WITHOUT kernel include paths: keep libc and kernel typedefs apart. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void host_note(const char *message) {
    puts(message);
    fflush(stdout);
}

/* The allocator's optional success logging is suppressed, assertions are not. */
int cprintf(const char *format, ...) {
    (void)format;
    return 0;
}

void __panic(const char *file, int line, const char *format, ...)
    __attribute__((noreturn));

void __panic(const char *file, int line, const char *format, ...) {
    va_list arguments;
    fprintf(stderr, "BUDDY_PANIC|%s|%d|", file, line);
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
    fflush(stderr);
    exit(86);
}
