#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// struct declarations

struct Location;
struct Token;
struct Node;
struct Vector;
struct StringBuilder;
struct Func;
struct Struct;
struct Type;
struct String;
struct Environment;

typedef struct Location Location;
typedef struct Token Token;
typedef struct Node Node;
typedef struct Vector Vec;
typedef struct StringBuilder StringBuilder;
typedef struct Map Map;
typedef struct Func Func;
typedef struct Struct Struct;
typedef struct Type Type;
typedef struct String String;
typedef struct Environment Environment;

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

struct Map {
    Vec *keys;
    Vec *values;
};

Map *map_new();
void map_put(Map *m, void *k, void *v);
void *map_find(Map *m, char *k);
void *map_pop(Map *m);

struct Environment {
    Map *map;
    Environment *next;
};

Environment *env_new(Environment *next);
Environment *env_next(Environment *e);
void env_push(Environment *e, char *k, void *v);
void *env_find(Environment *e, char *k);

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
    TK_CHAR,
    TK_STRING,
} Token_kind;

struct Token {
    Token_kind kind;
    Token *next;
    int val;
    char *str;
    Location *loc;
};

extern Vec *tokens;

void tokenize(char *p);

// parse

typedef enum {
    // expressions
    ND_NUM,
    ND_VAR,
    ND_SEQ,
    ND_ASGN,
    ND_COND,
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_MOD,
    ND_LSH,
    ND_RSH,
    ND_AND,
    ND_IOR,
    ND_XOR,
    ND_EQ,
    ND_NEQ,
    ND_LT,
    ND_LTE,
    ND_LAND,
    ND_LOR,
    ND_ADDEQ,
    ND_SUBEQ,
    ND_MULEQ,
    ND_DIVEQ,
    ND_MODEQ,
    ND_LSHEQ,
    ND_RSHEQ,
    ND_ANDEQ,
    ND_IOREQ,
    ND_XOREQ,
    ND_PREINCR,
    ND_POSTINCR,
    ND_PREDECR,
    ND_POSTDECR,
    ND_CALL,
    ND_ADDR,
    ND_DEREF,
    ND_SIZEOF,
    ND_NEG,
    ND_BCOMPL,
    ND_INDEX,
    ND_GVAR,
    ND_CHAR,
    ND_STRING,
    ND_ARRAY,
    ND_ATTR,

    // statements
    ND_VARDECL,
    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_DOWHILE,
    ND_BREAK,
    ND_CONTINUE,
    ND_BLOCK,
    ND_SWITCH,
    ND_CASE,
    ND_DEFAULT
} Node_kind;

struct Node {
    Node_kind kind;
    Node *cond;
    Node *lhs;
    Node *rhs;
    Node *body;
    Vec *block;
    int val;
    char *name;
    Type *type;
    Location *loc;
    Token *attr;
    bool is_extern;
};

struct Func {
    char *name;
    Vec *params;
    Vec *block;
    int offset;
    Type *ret_type;
    Map *global_vars;
    Location *loc;
    bool is_extern;
    bool is_varargs;
};

struct Struct {
    char *name;
    Vec *fields;
    Location *loc;
};

extern Vec *functions;
extern Map *global_vars;
extern Vec *string_literals;
extern Environment *builtin_aliases;

Environment *environment;

void parse();

Node *mknum(int v, Location *loc);

// type

typedef enum {
    TY_VOID,
    TY_INT,
    TY_CHAR,
    TY_PTR,
    TY_ARRAY,
    TY_STRUCT,
    TY_ENUM,
    TY_FUNC
} Type_kind;

struct Type {
    Type_kind ty;
    Type* ptr_to;
    int array_size;
    Struct *strct;
    bool enum_decl;
    Vec *enums;
};

extern Type *type_int;
extern Type *type_char;
extern Type *type_ptr_char;
extern Type *type_void;

Type *ptr_of(Type *type);
Type *array_of(Type *type, int len);
Type *func_returns(Type *type);
int type_size(Type *type);
bool is_int(Type *type);
bool is_integer(Type *type);
bool is_pointer(Type *type);
bool is_pointer_compat(Type *type);
bool is_enum(Type* type);
bool is_func(Type *type);
Type *coerce_pointer(Type *type);
Type *binary_int_op_result(Type *lhs, Type *rhs);
bool eq_type(Type *t1, Type *t2);

// semantic analysis

extern Map *func_env;

void sema_globals();
void sema_func(Func *func);

// codegen

void gen_globals();
void gen_func(Func *func);
