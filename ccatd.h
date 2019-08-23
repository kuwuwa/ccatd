#include <stdbool.h>

// struct declarations

typedef struct Token Token;
struct Token;

typedef struct Node Node;
struct Node;

typedef struct Vector Vec;
struct Vector;

// util

void error(char *fmt, ...);

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
} Node_kind;

typedef struct Node Node;
struct Node {
    Node_kind kind;
    Node *cond;     // ND_IF, ND_WHILE
    Node *lhs;      // binary operators
    Node *rhs;      // binary operators
    Node *body;     // ND_WHILE, ND_FOR
    Vec *block;     // ND_BLOCK, ND_CALL
    int val;        // ND_NUM
    int offset;     // ND_LVAR
    char *func;    // ND_CALL
    int len;        // ND_CALL
};

typedef struct Lvar Lvar;
struct Lvar {
    Lvar *next;
    char *name;
    int len;
    int offset;
};

extern Lvar *locals;

Vec* block();

// containers

Vec *vec_new();
void vec_push(Vec *vec, void *node);
int vec_len(Vec *vec);
void *vec_at(Vec *vec, int idx);

// codegen

extern Vec *code;

void gen(Node *node);
void gen_stmt(Node* node);
bool is_expr(Node_kind);
