#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// struct declarations

typedef struct Token Token;
struct Token;

typedef struct Node Node;
struct Node;

typedef struct Vector Vec;
struct Vector;

typedef struct Func Func;
struct Func;

typedef struct Type Type;
struct Type;

// containers

Vec *vec_new();
void vec_push(Vec *vec, void *node);
int vec_len(Vec *vec);
void *vec_at(Vec *vec, int idx);

// util

void error(char *fmt, ...);
void fnputs(FILE* fp, char *str, int n);

// tokenize

typedef enum {
    TK_KWD,
    TK_NUM,
    TK_IDT,
    TK_EOF,

    TK_RETURN,
    TK_IF,
    TK_ELSE,
    TK_WHILE,
    TK_FOR,
} Token_kind;

struct Token {
    Token_kind kind;
    Token *next;
    int val;
    char *str;
    int len;
};

Vec *tokenize(char *p);

extern Vec *tokens;

// parse

typedef enum {
    ND_NUM,
    ND_LVAR,
    ND_ASGN,

    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_EQ,
    ND_NEQ,
    ND_LT,
    ND_LTE,

    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_BLOCK,
    ND_CALL,

    ND_ADDR,
    ND_DEREF,
} Node_kind;

struct Node {
    Node_kind kind;
    Node *cond;     // ND_IF, ND_WHILE
    Node *lhs;      // binary operators
    Node *rhs;      // binary operators
    Node *body;     // ND_WHILE, ND_FOR
    Vec *block;     // ND_BLOCK, ND_CALL
    int val;        // ND_NUM, ND_LVAR
    char *name;     // ND_CALL, ND_LVAR
    int len;        // ND_CALL
    Type *type;
};

struct Func {
    char *name;
    int len;
    Vec *params;
    Vec *block;
    int offset;
    Type *ret_type;
};

struct Type {
    enum { TY_INT, TY_PTR } ty;
    Type* ptr_to;
};

Type *type_int;

Vec *locals;

Vec* parse();


// codegen

extern Vec *code;

void gen_func(Func *func);
