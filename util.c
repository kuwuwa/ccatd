#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool debug_flag = true;

void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void fnputs(FILE* fp, char *str, int n) {
    for (int i = 0; i < n; i++)
        putc(str[i], fp);
}

void debug(char *fmt, ...) {
    if (!debug_flag) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

