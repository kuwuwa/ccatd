#include "ccatd.h"

int loc_line = 1;
int loc_column = 1;
Vec *tokens;
Vec *string_literals;

void skip_column(char **p, int step) {
    *p += step;
    loc_column += step;
}

void skip_line(char **p) {
    (*p)++;
    loc_line++;
    loc_column = 1;
}

void skip_char(char **p, char ch) {
    if (ch == '\n') skip_line(p);
    else skip_column(p, 1);
}

Token *new_token(Token_kind kind, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = mkstr(str, len);

    tok->loc = calloc(1, sizeof(Location));
    tok->loc->line = loc_line;
    tok->loc->column = loc_column;

    return tok;
}

char *ops[46] = {
    "...",
    "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=", "^=", "|=",
    "&&", "||", "==", "!=", "<=", ">=", "<<", ">>", "->", "++", "--",
    "+", "-", "*", "/", "(", ")", "<", ">", "=", ";",
    "{", "}", ",", "&", "[", "]",
    "!", "?", ":", "|", "^", "%", ".", "~"
};

char *mem_op(char *p) {
    int ops_len = sizeof(ops) / sizeof(char*);

    int plen = strlen(p);
    for (int i = 0; i < ops_len; i++) {
        int ilen = strlen(ops[i]);
        if (plen >= ilen && !strncmp(p, ops[i], ilen))
            return ops[i];
    }
    return NULL;
}

char *kwds[17] = {
    "return", "if", "else", "while", "for", "typedef", "sizeof", "struct", "do",
    "break", "continue", "extern", "static", "switch", "case", "default", "enum"
};

Token *mem_kwd(char *p, int len) {
    int kwds_len = sizeof(kwds) / sizeof(char*);

    for (int i = 0; i < kwds_len; i++) {
        int ilen = strlen(kwds[i]);
        if (len == ilen && !strncmp(p, kwds[i], ilen))
            return new_token(TK_KWD, p, ilen);
    }
    return NULL;
}

void tokenize(char *p) {
    loc_line = 1;
    loc_column = 1;

    tokens = vec_new();

    while (*p) {
        if (isspace(*p)) {
            skip_char(&p, *p);
            continue;
        }

        if (!strncmp(p, "//", 2)) {
            while (*p != '\n') skip_column(&p, 1);
            skip_line(&p);
            continue;
        }

        if (!strncmp(p, "/*", 2)) {
            skip_column(&p, 2);
            while (*p && strncmp(p, "*/", 2))
                skip_char(&p, *p);
            if (!*p)
                error_loc2(loc_line, loc_column, "Closing comment \"*/\" expected");

            skip_column(&p, 2); // "*/"
            continue;
        }

        if (*p == '"') {
            StringBuilder *sb = strbld_new();
            while (*p && *p == '"') {
                skip_column(&p, 1); // '"'
                while (*p && *p != '"') {
                    if (*(p+1) && *p == '\\') {
                        skip_column(&p, 1); // '\\'
                        char escaped = *p == 'n' ? '\n'
                                     : *p == 'r' ? '\r'
                                     : *p == '0' ? '\0'
                                     : *p == '"' ? '"'
                                     : *p;
                        strbld_append(sb, escaped);
                        skip_char(&p, *p);
                    } else {
                        strbld_append(sb, *p);
                        skip_char(&p, *p);
                    }
                }
                if (*p) {
                    skip_column(&p, 1); // '"'
                    while (isspace(*p))
                        skip_char(&p, *p);
                }
            }

            if (!*p)
                error_loc2(loc_line, loc_column, "[parse] Closing double quote \"\\\"\" expected");

            char *content = strbld_build(sb);
            int len = strlen(content);
            vec_push(string_literals, content);
            vec_push(tokens, new_token(TK_STRING, content, len));
            continue;
        }

        if (*p == '\'') {
            Token *tk = new_token(TK_CHAR, NULL, 0);
            skip_column(&p, 1); // '\''
            if (*(p+1) && *p == '\\') {
                skip_column(&p, 1); // '\\'
                tk->val = *p == 'n' ? '\n'
                        : *p == 'r' ? '\r'
                        : *p == '0' ? '\0'
                        : *p == '\'' ? '\''
                        : *p;
            } else if (*p) tk->val = *p;
            else error_loc2(loc_line, loc_column, "[parse] unsupported character");

            skip_char(&p, *p); // content
            if (*p != '\'')
                error_loc2(loc_line, loc_column, "[parse] Closing single quote \"'\" expected");
            skip_char(&p, *p); // '\''
            vec_push(tokens, tk);
            continue;
        }

        char* op = mem_op(p);
        if (op != NULL) {
            int tlen = strlen(op);
            Token *token = new_token(TK_KWD, p, tlen);
            vec_push(tokens, token);
            skip_column(&p, tlen);
            continue;
        }

        if (isdigit(*p)) {
            Token *tk = new_token(TK_NUM, p, 0);
            char *q = p;
            tk->val = strtol(q, &q, 10);
            vec_push(tokens, tk);
            skip_column(&p, (q - p));
            continue;
        }

        char *q = p;
        while (isalpha(*q) || isdigit(*q) || *q == '_') q++;
        int len = q - p;
        Token *kwd = mem_kwd(p, len);
        if (kwd != NULL) {
            vec_push(tokens, kwd);
            skip_column(&p, len);
            continue;
        }

        if (len > 0) {
            vec_push(tokens, new_token(TK_IDT, p, q - p));
            skip_column(&p, q - p);
            continue;
        }

        error_loc2(loc_line, loc_column, "an unknown character was found: %d", *p);
    }
}
