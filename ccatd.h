
// util

void error(char *fmt, ...);

// tokenize

typedef enum {
    TK_KWD,
    TK_NUM,
    TK_IDT,
    TK_EOF,
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
} Node_kind;

typedef struct Node Node;

struct Node {
    Node_kind kind;
    Node *lhs;
    Node *rhs;
    int val; // ND_NUM
    int offset;
};

void prog();

// codegen

extern Node *code[];

void gen(Node *node);
