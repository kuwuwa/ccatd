#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// struct declarations

typedef struct Location Location;
struct Location;

typedef struct Token Token;
struct Token;

typedef struct Node Node;
struct Node;

typedef struct Vector Vec;
struct Vector;

typedef struct StringBuilder StringBuilder;
struct StringBuilder;

typedef struct Func Func;
struct Func;

typedef struct Type Type;
struct Type;

typedef struct String String;
struct String;

typedef struct Environment Environment;
struct Environment;

// containers

Vec *vec_new();
void vec_push(Vec *vec, void *node);
void *vec_pop(Vec *vec);
int vec_len(Vec *vec);
void *vec_at(Vec *vec, int idx);

StringBuilder *strbld_new();
char *strbld_build(StringBuilder *sb);
void strbld_append(StringBuilder *sb, char ch);
void strbld_append_str(StringBuilder *sb, char *ch);

// util

void error(char *fmt, ...);
void error_loc(Location *loc, char *fmt, ...);
void error_loc2(int line, int col, char *fmt, ...);
void debug(char *fmt, ...);
char *mkstr(char *ptr, int len);
char *escape_string(char* str);

// tokenize

struct Location {
    int line;
    int column;
};

typedef enum {
    TK_KWD,
    TK_NUM,
    TK_IDT,
    TK_EOF,
    TK_STRING,

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
    Location *loc;
};

void tokenize(char *p);

extern Vec *tokens;

// parse

typedef enum {
    // expressions
    ND_NUM,
    ND_LVAR,
    ND_SEQ,
    ND_ASGN,
    ND_COND,
    ND_LOR,
    ND_LAND,
    ND_IOR,
    ND_XOR,
    ND_AND,
    ND_EQ,
    ND_NEQ,
    ND_LT,
    ND_LTE,
    ND_LSH,
    ND_RSH,
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_MOD,
    ND_CALL,
    ND_ADDR,
    ND_DEREF,
    ND_SIZEOF,
    ND_NEG,
    ND_INDEX,
    ND_GVAR,
    ND_STRING,
    ND_ARRAY,

    // statements
    ND_VARDECL,
    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_BLOCK,
} Node_kind;

struct Node {
    Node_kind kind;
    Node *cond;     // ND_IF, ND_WHILE
    Node *lhs;      // binary operators
    Node *rhs;      // binary operators
    Node *body;     // ND_WHILE, ND_FOR
    Vec *block;     // ND_BLOCK, ND_CALL
    int val;        // ND_NUM, ND_LVAR
    char *name;     // ND_CALL, ND_LVAR, ND_STRING
    int len;        // ND_CALL, ND_STRING
    Type *type;
    Location *loc;
};

struct Func {
    char *name;
    Vec *params;
    Vec *block;
    int offset;
    Type *ret_type;
    Location *loc;
};

struct Environment {
    Vec *functions;
    Vec *globals;
    Vec *string_literals;
};

Environment *environment;

Vec *locals;

void parse();

Node *new_node_num(int v, Location *loc);

// type

struct Type {
    enum { TY_INT, TY_CHAR, TY_PTR, TY_ARRAY } ty;
    Type* ptr_to;
    int array_size;
};

extern Type *type_int;
extern Type *type_char;
extern Type *type_ptr_char;

Type *ptr_of(Type *type);

Type *array_of(Type *type, int len);

int type_size(Type *type);

bool is_int(Type *type);

bool is_integer(Type *type);

bool is_pointer_compat(Type *type);

Type *coerce_pointer(Type *type);

Type *binary_int_op_result(Type *lhs, Type *rhs);

// semantic analysis

extern Vec *func_env;

void sema_globals();

void sema_func(Func *func);

// codegen

void gen_globals();

void gen_func(Func *func);
