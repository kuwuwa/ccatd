#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

// tokenize
//
int loc_line = 1;
int loc_column = 1;

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
    if (ch == '\n')
        skip_line(p);
    else
        skip_column(p, 1);
}

Token *new_token(Token_kind kind, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;

    tok->loc = calloc(1, sizeof(Location));
    tok->loc->line = loc_line;
    tok->loc->column = loc_column;

    return tok;
}

void tokenize(char *p) {
    loc_line = 1;
    loc_column = 1;

    Vec *vec = vec_new();

    while (*p) {
        if (isspace(*p)) {
            skip_char(&p, *p);
            continue;
        }

        if (!memcmp(p, "//", 2)) {
            while (*p != '\n') skip_column(&p, 1);
            skip_line(&p);
            continue;
        }

        if (!memcmp(p, "/*", 2)) {
            while (p && memcmp(p, "*/", 2))
                skip_char(&p, *p);
            if (!*p) {
                error_loc2(loc_line, loc_column, "Closing comment \"*/\" expected");
            }
            skip_column(&p, 2); // "*/"
            continue;
        }

        if (*p == '"') {
            skip_column(&p, 1); // '"'
            char *q = p;
            while (*q && *q != '"') q++;
            if (!*q)
                error_loc2(loc_line, loc_column, "Closing quote \"\"\" expected");
            int len = q - p;

            String *str = calloc(1, sizeof(String));
            str->ptr = p;
            str->len = len;
            vec_push(environment->string_literals, str);
            vec_push(vec, new_token(TK_STRING, p, len));

            skip_column(&p, (q - p) + 1); // next to '"'
            continue;
        }

        if ((*p == '=' || *p == '!' || *p == '<' || *p == '>') && p+1 && *(p+1) == '=') {
            vec_push(vec, new_token(TK_KWD, p, 2));
            skip_column(&p, 2);
            continue;
        }

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' ||
                *p == '<' || *p == '>' || *p == '=' || *p == ';' ||
                *p == '{' || *p == '}' || *p == ',' || *p == '&' ||
                *p == '[' || *p == ']') {
            vec_push(vec, new_token(TK_KWD, p, 1));
            skip_column(&p, 1);
            continue;
        }

        if (isdigit(*p)) {
            Token *tk = new_token(TK_NUM, p, 0);
            char *q = p;
            tk->val = strtol(q, &q, 10);
            vec_push(vec, tk);
            skip_column(&p, (q - p));
            continue;
        }

        char *q = p;
        while (isalpha(*q) || isdigit(*q) || *q == '_') q++;
        int len = q - p;
        if (len == 6 && !memcmp("return", p, 6)) {
            vec_push(vec, new_token(TK_RETURN, p, 0));
            skip_column(&p, 6);
            continue;
        }
        if (len == 2 && !memcmp("if", p, 2)) {
            vec_push(vec, new_token(TK_IF, p, 0));
            skip_column(&p, 2);
            continue;
        }
        if (len == 4 && !memcmp("else", p, 4)) {
            vec_push(vec, new_token(TK_ELSE, p, 0));
            skip_column(&p, 4);
            continue;
        }
        if (len == 5 && !memcmp("while", p, 5)) {
            vec_push(vec, new_token(TK_WHILE, p, 0));
            skip_column(&p, 5);
            continue;
        }
        if (len == 3 && !memcmp("for", p, 3)) {
            vec_push(vec, new_token(TK_FOR, p, 0));
            skip_column(&p, 3);
            continue;
        }
        if (len == 6 && !memcmp("sizeof", p, 6)) {
            vec_push(vec, new_token(TK_KWD, p, 6));
            skip_column(&p, 6);
            continue;
        }

        if (len > 0) {
            vec_push(vec, new_token(TK_IDT, p, q - p));
            skip_column(&p, q - p);
            continue;
        }

        error_loc2(loc_line, loc_column, "an unknown character was found");
    }

    tokens = vec;
}

Vec *tokens;
int index = 0;

Token *consume_keyword(char *str) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != TK_KWD || strlen(str) != tk->len || memcmp(str, tk->str, tk->len))
        return NULL;
    index++;
    return tk;
}

bool lookahead_keyword(char *str) {
    Token *tk = vec_at(tokens, index);
    return tk->kind == TK_KWD && strlen(str) == tk->len && !memcmp(str, tk->str, tk->len);
}

Token *consume(Token_kind kind) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != kind)
        return NULL;
    index++;
    return tk;
}

Token *lookahead(Token_kind kind) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != kind)
        return NULL;
    return tk;
}

Token *lookahead_any() {
    if (index >= vec_len(tokens))
        return NULL;
    return vec_at(tokens, index);
}

Token *expect(Token_kind kind) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != kind)
        error_loc(tk->loc, "%s expected\n", kind);
    index++;
    return tk;
}

void expect_keyword(char* str) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != TK_KWD || strlen(str) != tk->len || memcmp(str, tk->str, tk->len))
        error_loc(tk->loc, "\"%s\" expected", str);
    index++;
}

bool lookahead_type(char* str) {
    int len = strlen(str);
    Token *tk = lookahead(TK_IDT);
    return tk != NULL && tk->len == len && !memcmp(tk->str, str, len);
}

bool lookahead_any_type() {
    return lookahead_type("int") || lookahead_type("char");
}

Type *consume_type() {
    if (lookahead_type("int")) {
        index++;
        return type_int;
    } else if (lookahead_type("char")) {
        index++;
        return type_char;
    }
    return NULL;
}


// parse

Node *new_op(Node_kind kind, Node *lhs, Node *rhs, Location *loc) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    node->loc = loc;
    return node;
}

Node *new_node_num(int v, Location *loc) {
    Node *node = calloc(1, sizeof(Node));
    node->type = type_int;
    node->kind = ND_NUM;
    node->val = v;
    node->loc = loc;
    return node;
}

void toplevel();
Func *func(Type *ty, Token *name);
Type *type();
void type_array(Type**);
Vec *params();
Node *stmt();
Vec *block();
Node *expr();
Node *equal();
Node *comp();
Node *add();
Node *mul();
Node *unary();
Node *term();
Vec *args();

Node *find_var(Token*);
Node *push_lvar(Type*, Token*);

void parse() {
    int len = vec_len(tokens);
    while (index < len)
        toplevel();
}

void toplevel() {
    Type *ty = type();
    Token *tk = expect(TK_IDT);
    if (consume_keyword("(")) {
        locals = vec_new();
        Func *f = func(ty, tk);
        vec_push(environment->functions, f);
    } else {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_GVAR;
        node->name = tk->str;
        node->len = tk->len;
        node->loc = tk->loc;

        while (consume_keyword("[")) {
            Token *len = expect(TK_NUM);
            expect_keyword("]");

            ty = array_of(ty, len->val);
        }
        expect_keyword(";");

        node->type = ty;
        vec_push(environment->globals, node);
    }
}

Func *func(Type *ty, Token *name) {
    Vec *par = params();
    Vec *blk = block();

    Func *func = calloc(1, sizeof(Func));
    func->name = name->str;
    func->len = name->len;
    func->params = par;
    func->block = blk;
    func->loc = name->loc;

    func->ret_type = ty;
    return func;
}

Type *type() {
    Type *typ = consume_type();
    if (typ == NULL)
        error_loc(lookahead_any()->loc, "unknown type");

    while (consume_keyword("*"))
        typ = ptr_of(typ);
    return typ;
}

Vec *params() {
    Vec *vec = vec_new();
    if (consume_keyword(")"))
        return vec;

    Type *ty = type();
    Token *tk = expect(TK_IDT);
    Node *param_0 = push_lvar(ty, tk);
    vec_push(vec, param_0);
    while (!consume_keyword(")")) {
        expect_keyword(",");
        Type *ty = type();
        Token *tk = expect(TK_IDT);
        Node *param_i = push_lvar(ty, tk);
        vec_push(vec, param_i);
    }
    return vec;
}

Vec *block() {
    int revert_locals_len = vec_len(locals);

    Vec *vec = vec_new();
    expect_keyword("{");
    while (!consume_keyword("}"))
        vec_push(vec, stmt());

    while (vec_len(locals) > revert_locals_len)
        vec_pop(locals);

    return vec;
}

Node *stmt() {
    Token *tk;
    Node *node;
    if ((tk = consume(TK_RETURN))) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_RETURN;
        node->lhs = expr();
        node->loc = tk->loc;
        expect_keyword(";");
    } else if ((tk = consume(TK_IF))) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_IF;
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->lhs = stmt();
        node->rhs = consume(TK_ELSE) ? stmt() : NULL;
        node->loc = tk->loc;
    } else if ((tk = consume(TK_WHILE))) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_WHILE;
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->body = stmt();
        node->loc = tk->loc;
    } else if ((tk = consume(TK_FOR))) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_FOR;
        node->loc = tk->loc;
        expect_keyword("(");
        if (!consume_keyword(";")) {
            node->lhs = expr();
            expect_keyword(";");
        }
        if (!consume_keyword(";")) {
            node->cond = expr();
            expect_keyword(";");
        }
        if (!consume_keyword(")")) {
            node->rhs = expr();
            expect_keyword(")");
        }
        node->body = stmt();
    } else if (lookahead_keyword("{")) {
        Vec *vec = block();
        node = calloc(1, sizeof(Node));
        node->kind = ND_BLOCK;
        node->block = vec;
    } else if (lookahead_any_type()) {
        Type *typ = type();
        Token *id = expect(TK_IDT);
        type_array(&typ);
        Node *rhs = consume_keyword("=") ? expr() : NULL;
        expect_keyword(";");

        Node *lhs = find_var(id);
        if (lhs == NULL)
            lhs = push_lvar(typ, id);

        node = new_op(ND_VARDECL, lhs, rhs, id->loc);
    } else {
        node = expr();
        expect_keyword(";");
    }
    return node;
}

void type_array(Type **t) {
    while (consume_keyword("[")) {
        Token *len = consume(TK_NUM);
        *t = array_of(*t, len->val);
        expect_keyword("]");
    }
}

Node *expr() {
    Node *node = equal();
    Token *tk;
    if ((tk = consume_keyword("=")))
        node = new_op(ND_ASGN, node, expr(), tk->loc);
    return node;
}

Node *equal() {
    Node *node = comp();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("==")))
            node = new_op(ND_EQ, node, comp(), tk->loc);
        else if ((tk = consume_keyword("!=")))
            node = new_op(ND_NEQ, node, comp(), tk->loc);
        else break;
    }
    return node;
}

Node *comp() {
    Node *node = add();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("<")))
            node = new_op(ND_LT, node, add(), tk->loc);
        else if ((tk = consume_keyword("<=")))
            node = new_op(ND_LTE, node, add(), tk->loc);
        else if ((tk = consume_keyword(">")))
            node = new_op(ND_LT, add(), node, tk->loc);
        else if ((tk = consume_keyword(">=")))
            node = new_op(ND_LTE, add(), node, tk->loc);
        else break;
    }
    return node;
}

Node *add() {
    Node *node = mul();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("+")))
            node = new_op(ND_ADD, node, mul(), tk->loc);
        else if ((tk = consume_keyword("-")))
            node = new_op(ND_SUB, node, mul(), tk->loc);
        else break;
    }
    return node;
}

Node *mul() {
    Node *node = unary();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("*")))
            node = new_op(ND_MUL, node, unary(), tk->loc);
        else if ((tk = consume_keyword("/")))
            node = new_op(ND_DIV, node, unary(), tk->loc);
        else break;
    }
    return node;
}

Node *unary() {
    Token *tk;
    if ((tk = consume_keyword("sizeof"))) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_SIZEOF;
        node->lhs = unary();
        node->loc = tk->loc;
        return node;
    }
    if (consume_keyword("+"))
        return term();
    if ((tk = consume_keyword("-")))
        return new_op(ND_SUB, new_node_num(0, NULL), term(), tk->loc);
    if ((tk = consume_keyword("&"))) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_ADDR;
        node->lhs = unary();
        node->loc = tk->loc;
        return node;
    }
    if ((tk = consume_keyword("*"))) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_DEREF;
        node->lhs = unary();
        node->loc = tk->loc;
        return node;
    }
    return term();
}

Node *term() {
    if (consume_keyword("(")) {
        Node *node = expr();
        expect_keyword(")");
        return node;
    }

    Node *node = NULL;
    Token *tk = consume(TK_IDT);
    if (tk) {
        if (consume_keyword("(")) { // CALL
            Vec *vec = args();

            node = calloc(1, sizeof(Node));
            node->kind = ND_CALL;
            node->name = tk->str;
            node->len = tk->len;
            node->block = vec;
            node->loc = tk->loc;
        } else { // variable
            node = find_var(tk);
            if (node == NULL) {
                error_loc(tk->loc, "variable not defined");
            }
        }
    } else if ((tk = consume(TK_NUM))) { // number
        node = new_node_num(tk->val, tk->loc);
    } else if ((tk = consume(TK_STRING))) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_STRING;
        node->name = tk->str;
        node->len = tk->len;
        node->type = type_ptr_char;
    }

    if (node == NULL) {
        if (tk == NULL)
            tk = lookahead_any();
        error_loc(tk->loc, "invalid expression or statement");
    }

    while ((tk = consume_keyword("["))) {
        Node *index_node = expr();
        expect_keyword("]");

        Node *add_node = new_op(ND_ADD, node, index_node, tk->loc);

        Node *next_node = calloc(1, sizeof(Node));
        next_node->kind = ND_DEREF;
        next_node->lhs = add_node;

        node = next_node;
    }

    return node;
}

Vec *args() {
    Vec *vec = vec_new();
    if (consume_keyword(")"))
        return vec;
    vec_push(vec, expr());
    while (!consume_keyword(")")) {
        expect_keyword(",");
        vec_push(vec, expr());
    }
    return vec;
}

Node *find_var(Token *token) {
    if (token->kind != TK_IDT)
        error("find_lvar: lvar expected\n");

    int locals_len = vec_len(locals);
    for (int i = 0; i < locals_len; i++) {
        Node *var = vec_at(locals, i);
        if (token->len == var->len && memcmp(token->str, var->name, token->len) == 0)
            return var;
    }

    int globals_len = vec_len(environment->globals);
    for (int i = 0; i < globals_len; i++) {
        Node *var = vec_at(environment->globals, i);
        if (token->len == var->len && memcmp(token->str, var->name, token->len) == 0)
            return var;
    }

    return NULL;
}

Node *push_lvar(Type *ty, Token *tk) {
    Node *var = calloc(1, sizeof(Node));
    var->kind = ND_LVAR;
    var->name = tk->str;
    var->type = ty;
    var->len = tk->len;

    vec_push(locals, var);
    return var;
}
