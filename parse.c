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
                *p == '{' || *p == '}') {
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

bool consume_keyword(char* str) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != TK_KWD || strlen(str) != tk->len || strncmp(str, tk->str, tk->len))
        return false;
    index++;
    return true;
}

Token *consume(Token_kind kind) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != kind)
        return NULL;
    index++;
    return tk;
}

void expect_keyword(char* str) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != TK_KWD || strlen(str) != tk->len || strncmp(str, tk->str, tk->len))
        error("expected \"%s\"", str);
    index++;
}

// parse

Vec *code;
Lvar *locals = NULL;

Node *new_op(Node_kind kind, Node* lhs, Node* rhs) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_node_num(int v) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = v;
    return node;
}

Vec *stmt();

Node *expr();
Node *equal();
Node *comp();
Node *add();
Node *mul();
Node *unary();
Node *term();

Vec *prog() {
    return stmt();
}

Vec *stmt() {
    Vec *vec = vec_new();
    if (!consume_keyword("{")) {
        vec_push(vec, expr());
        expect_keyword(";");
        return vec;
    }
    for (Node *node; !consume_keyword("}"); vec_push(vec, node)) {
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
            node->cls1 = stmt();
            node->cls2 = consume(TK_ELSE) ? stmt() : NULL;
        } else if (consume(TK_WHILE)) {
            node = calloc(1, sizeof(Node));
            node->kind = ND_WHILE;
            expect_keyword("(");
            node->cond = expr();
            expect_keyword(")");
            node->cls1 = stmt();
        } else {
            node = expr();
            expect_keyword(";");
        }
    }

    return vec;
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
    if (consume_keyword("+"))
        return term();
    if (consume_keyword("-"))
        return new_op(ND_SUB, new_node_num(0), term());
    return term();
}

Lvar *find_lvar(Token *token) {
    for (Lvar *var = locals; var; var = var->next) {
        if (token->len == var->len && memcmp(token->str, var->name, token->len) == 0)
            return var;
    }
    return NULL;
}

Node *term() {
    if (consume_keyword("(")) {
        Node *node = expr();
        expect_keyword(")");
        return node;
    }

    Token *tk = consume(TK_IDT);
    if (tk) {
        Lvar *var = find_lvar(tk);
        if (var == NULL) {
            var = calloc(1, sizeof(Lvar));
            var->name = tk->str;
            var->len = tk->len;
            var->offset = locals->offset + 8;
            var->next = locals;

            locals = var;
        }

        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_LVAR;
        node->offset = var->offset;
        return node;
    }

    tk = consume(TK_NUM);
    if (!tk) {
        error("'(' <expr> ')' | <ident> | <num> expected");
    }

    Node *node = new_node_num(tk->val);
    return node;
}
