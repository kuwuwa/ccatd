#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

// tokenize

Token *new_token(Token_kind kind, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    return tok;
}

Vec *tokenize(char *p) {
    Vec *vec = vec_new();

    while (*p) {
        if (isspace(*p)) {
            p++;
            continue;
        }

        if ((*p == '=' || *p == '!' || *p == '<' || *p == '>') && p+1 && *(p+1) == '=') {
            vec_push(vec, new_token(TK_KWD, p, 2));
            p += 2;
            continue;
        }

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' ||
                *p == '<' || *p == '>' || *p == '=' || *p == ';' ||
                *p == '{' || *p == '}' || *p == ',' || *p == '&' ||
                *p == '[' || *p == ']') {
            vec_push(vec, new_token(TK_KWD, p, 1));
            p += 1;
            continue;
        }

        if (isdigit(*p)) {
            Token *tk = new_token(TK_NUM, p, 0);
            tk->val = strtol(p, &p, 10);
            vec_push(vec, tk);
            continue;
        }

        char *q = p;
        while (isalpha(*q) || isdigit(*q) || *q == '_') q++;
        int len = q - p;
        if (len == 6 && !memcmp("return", p, 6)) {
            vec_push(vec, new_token(TK_RETURN, p, 0));
            p += 6;
            continue;
        }
        if (len == 2 && !memcmp("if", p, 2)) {
            vec_push(vec, new_token(TK_IF, p, 0));
            p += 2;
            continue;
        }
        if (len == 4 && !memcmp("else", p, 4)) {
            vec_push(vec, new_token(TK_ELSE, p, 0));
            p += 4;
            continue;
        }
        if (len == 5 && !memcmp("while", p, 5)) {
            vec_push(vec, new_token(TK_WHILE, p, 0));
            p += 5;
            continue;
        }
        if (len == 3 && !memcmp("for", p, 3)) {
            vec_push(vec, new_token(TK_FOR, p, 0));
            p += 3;
            continue;
        }
        if (len == 6 && !memcmp("sizeof", p, 6)) {
            vec_push(vec, new_token(TK_KWD, p, 6));
            p += 6;
            continue;
        }

        if (len > 0) {
            vec_push(vec, new_token(TK_IDT, p, q - p));
            p = q;
            continue;
        }

        error("an unknown character was found");
    }

    return vec;
}

Vec *tokens;
int index = 0;

bool consume_keyword(char *str) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != TK_KWD || strlen(str) != tk->len || memcmp(str, tk->str, tk->len))
        return false;
    index++;
    return true;
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

Token *expect(Token_kind kind) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != kind)
        error("%s expected\n", kind);
    index++;
    return tk;
}

void expect_keyword(char* str) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != TK_KWD || strlen(str) != tk->len || memcmp(str, tk->str, tk->len))
        error("\"%s\" expected", str);
    index++;
}

bool lookahead_int_type() {
    Token *tk = lookahead(TK_IDT);
    return tk != NULL && tk->len == 3 && !memcmp(tk->str, "int", 3);
}

bool consume_int_type() {
    bool consumed = lookahead_int_type();
    if (consumed)
        index++;
    return consumed;
}

void expect_int_type() {
    if (consume_int_type())
        return;
    error("`int' expected");
}

// parse

Node *new_op(Node_kind kind, Node* lhs, Node* rhs) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_node_num(int v) {
    Node *node = calloc(1, sizeof(Node));
    node->type = type_int;
    node->kind = ND_NUM;
    node->val = v;
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
    environment = calloc(1, sizeof(Environment));
    environment->functions = vec_new();
    environment->globals = vec_new();

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

    func->ret_type = ty;
    return func;
}

Type *type() {
    expect_int_type();
    Type *typ = calloc(1, sizeof(Type));
    typ->ty = TY_INT;
    while (consume_keyword("*")) {
        Type *u = calloc(1, sizeof(Type));
        u->ty = TY_PTR;
        u->ptr_to = typ;
        typ = u;
    }
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
    Vec *vec = vec_new();
    expect_keyword("{");
    while (!consume_keyword("}"))
        vec_push(vec, stmt());
    return vec;
}

Node *stmt() {
    Node *node;
    if (consume(TK_RETURN)) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_RETURN;
        node->lhs = expr();
        expect_keyword(";");
    } else if (consume(TK_IF)) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_IF;
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->lhs = stmt();
        node->rhs = consume(TK_ELSE) ? stmt() : NULL;
    } else if (consume(TK_WHILE)) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_WHILE;
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->body = stmt();
    } else if (consume(TK_FOR)) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_FOR;
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
    } else if (lookahead_int_type()) {
        Type *typ = type();
        Token *id = expect(TK_IDT);
        type_array(&typ);
        Node *rhs = consume_keyword("=") ? expr() : NULL;
        expect_keyword(";");

        Node *lhs = find_var(id);
        if (lhs == NULL)
            lhs = push_lvar(typ, id);

        node = new_op(ND_VARDECL, lhs, rhs);
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
    if (consume_keyword("="))
        node = new_op(ND_ASGN, node, expr());
    return node;
}

Node *equal() {
    Node *node = comp();
    for (;;) {
        if (consume_keyword("=="))
            node = new_op(ND_EQ, node, comp());
        else if (consume_keyword("!="))
            node = new_op(ND_NEQ, node, comp());
        else break;
    }
    return node;
}

Node *comp() {
    Node *node = add();
    for (;;) {
        if (consume_keyword("<"))
            node = new_op(ND_LT, node, add());
        else if (consume_keyword("<="))
            node = new_op(ND_LTE, node, add());
        else if (consume_keyword(">"))
            node = new_op(ND_LT, add(), node);
        else if (consume_keyword(">="))
            node = new_op(ND_LTE, add(), node);
        else break;
    }
    return node;
}

Node *add() {
    Node *node = mul();
    for (;;) {
        if (consume_keyword("+"))
            node = new_op(ND_ADD, node, mul());
        else if (consume_keyword("-"))
            node = new_op(ND_SUB, node, mul());
        else break;
    }
    return node;
}

Node *mul() {
    Node *node = unary();
    for (;;) {
        if (consume_keyword("*"))
            node = new_op(ND_MUL, node, unary());
        else if (consume_keyword("/"))
            node = new_op(ND_DIV, node, unary());
        else break;
    }
    return node;
}

Node *unary() {
    if (consume_keyword("sizeof")) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_SIZEOF;
        node->lhs = unary();
        return node;
    }
    if (consume_keyword("+"))
        return term();
    if (consume_keyword("-"))
        return new_op(ND_SUB, new_node_num(0), term());
    if (consume_keyword("&")) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_ADDR;
        node->lhs = unary();
        return node;
    }
    if (consume_keyword("*")) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_DEREF;
        node->lhs = unary();
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

    Node *node;
    Token *tk = consume(TK_IDT);
    if (tk) {
        if (consume_keyword("(")) { // CALL
            Vec *vec = args();

            node = calloc(1, sizeof(Node));
            node->kind = ND_CALL;
            node->name = tk->str;
            node->len = tk->len;
            node->block = vec;
        } else { // variable
            node = find_var(tk);
            if (node == NULL) {
                fprintf(stderr, "variable not defined: ");
                fnputs(stdout, tk->str, tk->len);
                error("\n");
            }
        }
    } else if ((tk = consume(TK_NUM))) { // number
        node = new_node_num(tk->val);
    }

    if (!node)
        error("invalid expression or statement");

    while (consume_keyword("[")) {
        Node *index_node = expr();
        expect_keyword("]");

        Node *add_node = new_op(ND_ADD, node, index_node);

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
