#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// tokenize

typedef enum {
    TK_KWD,
    TK_NUM,
    TK_EOF,
} Token_kind;

typedef struct Token Token;

struct Token {
    Token_kind kind;
    Token *next;
    int val;
    char *str;
};

void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

Token *new_token(Token_kind kind, Token *cur, char *str) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
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

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')') {
            cur = new_token(TK_KWD, cur, p++);
            continue;
        }

        if (isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p);
            cur->val = strtol(p, &p, 10);
            continue;
        }

        error("an unknown character was found");
    }

    new_token(TK_EOF, cur, p);
    return head.next;
}

Token* token;

bool consume(char op) {
    if (token->kind != TK_KWD || token->str[0] != op)
        return false;
    token = token->next;
    return true;
}

void expect(char op) {
    if (token->kind != TK_KWD || token->str[0] != op)
        error("expected '%c'", op);
    token = token->next;
}

int expect_number() {
    if (token->kind != TK_NUM)
        error("not a number");
    int val = token->val;
    token = token->next;
    return val;
}

bool at_eof() {
    return token->kind == TK_EOF;
}

// parse

typedef enum {
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_NUM,
} Node_kind;

typedef struct Node Node;

struct Node {
    Node_kind kind;
    Node *lhs;
    Node *rhs;
    int val; // ND_NUM
};

Node *new_node(Node_kind kind, Node* lhs, Node* rhs) {
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

Node *expr();
Node *mul();
Node *unary();
Node *term();

Node *expr() {
    Node *node = mul();
    for (;;) {
        if (consume('+'))
            node = new_node(ND_ADD, node, mul());
        else if (consume('-'))
            node = new_node(ND_SUB, node, mul());
        else break;
    }
    return node;
}

Node *mul() {
    Node *node = unary();
    for (;;) {
        if (consume('*'))
            node = new_node(ND_MUL, node, unary());
        else if (consume('/'))
            node = new_node(ND_DIV, node, unary());
        else break;
    }
    return node;
}

Node *unary() {
    if (consume('+'))
        return term();
    if (consume('-'))
        return new_node(ND_SUB, new_node_num(0), term());
    return term();
}

Node *term() {
    if (consume('(')) {
        Node *node = expr();
        expect(')');
        return node;
    }

    return new_node_num(expect_number());
}

// generate
//
void gen(Node *node) {
    if (node->kind == ND_NUM) {
        printf("  push %d\n", node->val);
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind) {
        case ND_ADD: 
            printf("  add rax, rdi\n");
            break;
        case ND_SUB: 
            printf("  sub rax, rdi\n");
            break;
        case ND_MUL: 
            printf("  imul rax, rdi\n");
            break;
        case ND_DIV: 
            printf("  cqo\nidiv rdi\n");
            break;
    }

    printf("  push rax\n");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid number of argument(s)\n");
        return 1;
    }

    token = tokenize(argv[1]);
    Node* node = expr();

    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");
    gen(node);
    printf("  pop rax\n");
    printf("  ret\n");
    return 0;
}
