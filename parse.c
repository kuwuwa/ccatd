#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

// tokenize

Token *new_token(Token_kind kind, Token *cur, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

Token *tokenize(char *p) {
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        if (isspace(*p)) {
            p++;
            continue;
        }

        if ((*p == '=' || *p == '!' || *p == '<' || *p == '>') && p+1 && *(p+1) == '=') {
            cur = new_token(TK_KWD, cur, p, 2);
            p += 2;
            continue;
        }

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' ||
                *p == '<' || *p == '>' || *p == '=' || *p == ';') {
            cur = new_token(TK_KWD, cur, p++, 1);
            continue;
        }

        if (isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p, 0);
            cur->val = strtol(p, &p, 10);
            continue;
        }

        char *q = p;
        while (isalpha(*q)) q++;
        if (p < q) {
            cur = new_token(TK_IDT, cur, p, q - p);
            p = q;
            continue;
        }

        error("an unknown character was found");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}

Token* token;

bool consume_keyword(char* str) {
    if (token->kind != TK_KWD || strlen(str) != token->len ||
            strncmp(str, token->str, token->len))
        return false;
    token = token->next;
    return true;
}

Token *consume(Token_kind kind) {
    if (token->kind != kind)
        return NULL;
    Token *tk = token;
    token = token->next;
    return tk;
}

void expect(char* str) {
    if (token->kind != TK_KWD || strlen(str) != token->len ||
            strncmp(str, token->str, token->len))
        error("expected \"%s\"", str);
    token = token->next;
}

bool at_eof() {
    return token->kind == TK_EOF;
}

// parse

Node *code[101];
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

Node *stmt();
Node *expr();
Node *equal();
Node *comp();
Node *add();
Node *mul();
Node *unary();
Node *term();

void prog() {
    int i = 0;
    while (i < 100 && !at_eof())
        code[i++] = stmt();
    code[i] = NULL;
}

Node *stmt() {
    Node *node = expr();
    expect(";");
    return node;
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
        if (consume_keyword("==")) {
            node = new_op(ND_EQ, node, comp());
        } else if (consume_keyword("!="))
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
        expect(")");
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
