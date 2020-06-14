#include "ccatd.h"

bool debug_flag = true;

void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_loc(Location *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[l:%d,c:%d] ", loc->line, loc->column);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_loc2(int line, int col, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[l:%d,c:%d] ", line, col);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void debug(char *fmt, ...) {
    if (!debug_flag) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

char *escape_string(char *str) {
    int len = strlen(str);
    StringBuilder *sb = strbld_new();
    for (int i = 0; i < len; i++) {
        char ch = str[i];
        if (ch == '\n') strbld_append_str(sb, "\\n");
        else if (ch == '"') strbld_append_str(sb, "\\\"");
        else strbld_append(sb, ch);
    }
    return strbld_build(sb);
}

char *mkstr(char *str, int len) {
    char *p = calloc(len+1, sizeof(char));
    strncpy(p, str, len);
    return p;
}
