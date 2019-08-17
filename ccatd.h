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
    int len;
};

Token *tokenize(char *p);

extern Token *token;

// parse

typedef enum {
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_NUM,
    ND_EQ,
    ND_NEQ,
    ND_LT,
    ND_LTE
} Node_kind;

typedef struct Node Node;

struct Node {
    Node_kind kind;
    Node *lhs;
    Node *rhs;
    int val; // ND_NUM
};

Node *expr();

// codegen

void gen(Node *node);
