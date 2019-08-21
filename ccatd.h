#include <stdbool.h>

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
    TK_WHILE
} Token_kind;

typedef struct Token Token;

struct Token {
    Token_kind kind;
    Token *next;
    int val;
    char *str;
    int len;
};

Token *tokenize(char *p);

extern Token *token;

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
} Node_kind;

typedef struct Node Node;
struct Node {
    Node_kind kind;
    Node *lhs;
    Node *rhs;
    Node *cond;
    int val; // ND_NUM
    int offset;
};

typedef struct Lvar Lvar;
struct Lvar {
    Lvar *next;
    char *name;
    int len;
    int offset;
};

extern Lvar *locals;

void prog();

// codegen

extern Node *code[];

void gen(Node *node);
void gen_stmt(Node* node);
bool is_expr(Node_kind);
