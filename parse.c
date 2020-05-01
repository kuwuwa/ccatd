#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

// tokenize
//
int loc_line = 1;
int loc_column = 1;

void skip_column(char **p, int step) {
    *p += step;
    loc_column += step;
}

void skip_line(char **p) {
    (*p)++;
    loc_line++;
    loc_column = 1;
}

void skip_char(char **p, char ch) {
    if (ch == '\n')
        skip_line(p);
    else
        skip_column(p, 1);
}

Token *new_token(Token_kind kind, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = mkstr(str, len);

    tok->loc = calloc(1, sizeof(Location));
    tok->loc->line = loc_line;
    tok->loc->column = loc_column;

    return tok;
}

int mem_str(char *p, char *ids[], int idslen) {
    int plen = strlen(p);
    for (int i = 0; i < idslen; i++) {
        int ilen = strlen(ids[i]);
        if (plen >= ilen && !strncmp(p, ids[i], ilen))
            return i;
    }
    return -1;
}

void tokenize(char *p) {
    loc_line = 1;
    loc_column = 1;

    Vec *vec = vec_new();

    while (*p) {
        if (isspace(*p)) {
            skip_char(&p, *p);
            continue;
        }

        if (!memcmp(p, "//", 2)) {
            while (*p != '\n') skip_column(&p, 1);
            skip_line(&p);
            continue;
        }

        if (!memcmp(p, "/*", 2)) {
            while (p && memcmp(p, "*/", 2))
                skip_char(&p, *p);
            if (!*p) {
                error_loc2(loc_line, loc_column, "Closing comment \"*/\" expected");
            }
            skip_column(&p, 2); // "*/"
            continue;
        }

        if (*p == '"') {
            skip_column(&p, 1); // '"'
            char *q = p;
            while (*q && *q != '"') q++;
            if (!*q)
                error_loc2(loc_line, loc_column, "Closing quote \"\"\" expected");
            int len = q - p;

            vec_push(environment->string_literals, mkstr(p, len));
            vec_push(vec, new_token(TK_STRING, p, len));

            skip_column(&p, (q - p) + 1); // next to '"'
            continue;
        }

        char *tkids[] = {
            "&&", "||",
            "==", "!=", "<=", ">=",
            "+", "-", "*", "/", "(", ")", "<", ">", "=", ";",
            "{", "}", ",", "&", "[", "]",
            "!",
        };
        int idx = mem_str(p, tkids, sizeof(tkids) / sizeof(char*));
        if (idx >= 0) {
            int tlen = strlen(tkids[idx]);
            Token *token = new_token(TK_KWD, p, tlen);
            vec_push(vec, token);
            skip_column(&p, tlen);
            continue;
        }

        if (isdigit(*p)) {
            Token *tk = new_token(TK_NUM, p, 0);
            char *q = p;
            tk->val = strtol(q, &q, 10);
            vec_push(vec, tk);
            skip_column(&p, (q - p));
            continue;
        }

        char *q = p;
        while (isalpha(*q) || isdigit(*q) || *q == '_') q++;
        int len = q - p;
        if (len == 6 && !memcmp("return", p, 6)) {
            vec_push(vec, new_token(TK_RETURN, p, 0));
            skip_column(&p, 6);
            continue;
        }
        if (len == 2 && !memcmp("if", p, 2)) {
            vec_push(vec, new_token(TK_IF, p, 0));
            skip_column(&p, 2);
            continue;
        }
        if (len == 4 && !memcmp("else", p, 4)) {
            vec_push(vec, new_token(TK_ELSE, p, 0));
            skip_column(&p, 4);
            continue;
        }
        if (len == 5 && !memcmp("while", p, 5)) {
            vec_push(vec, new_token(TK_WHILE, p, 0));
            skip_column(&p, 5);
            continue;
        }
        if (len == 3 && !memcmp("for", p, 3)) {
            vec_push(vec, new_token(TK_FOR, p, 0));
            skip_column(&p, 3);
            continue;
        }
        if (len == 6 && !memcmp("sizeof", p, 6)) {
            vec_push(vec, new_token(TK_KWD, p, 6));
            skip_column(&p, 6);
            continue;
        }

        if (len > 0) {
            vec_push(vec, new_token(TK_IDT, p, q - p));
            skip_column(&p, q - p);
            continue;
        }

        error_loc2(loc_line, loc_column, "an unknown character was found");
    }

    tokens = vec;
}

Vec *tokens;
int index = 0;

Token *lookahead_any() {
    if (index >= vec_len(tokens))
        return NULL;
    return vec_at(tokens, index);
}

Token *lookahead(Token_kind kind) {
    Token *tk = lookahead_any();
    if (tk->kind != kind)
        return NULL;
    return tk;
}

bool lookahead_keyword(char *str) {
    int len = strlen(str);
    Token *tk = lookahead_any();
    return tk != NULL && tk->kind == TK_KWD && len == strlen(tk->str) && !strcmp(str, tk->str);
}

Token *consume_keyword(char *str) {
    Token *tk = lookahead(TK_KWD);
    if (tk == NULL || tk->kind != TK_KWD || strlen(str) != strlen(tk->str) || strcmp(str, tk->str))
        return NULL;
    index++;
    return tk;
}

Token *consume(Token_kind kind) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != kind)
        return NULL;
    index++;
    return tk;
}

Token *expect(Token_kind kind) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != kind)
        error_loc(tk->loc, "%s expected\n", kind);
    index++;
    return tk;
}

void expect_keyword(char* str) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != TK_KWD || strlen(str) != strlen(tk->str) || strcmp(str, tk->str))
        error_loc(tk->loc, "\"%s\" expected", str);
    index++;
}

bool lookahead_type(char* str) {
    Token *tk = lookahead(TK_IDT);
    return tk != NULL && strlen(str) == strlen(tk->str) && !strcmp(str, tk->str);
}

bool lookahead_any_type() {
    return lookahead_type("int") || lookahead_type("char");
}

Type *consume_type() {
    if (lookahead_type("int")) {
        index++;
        return type_int;
    } else if (lookahead_type("char")) {
        index++;
        return type_char;
    }
    return NULL;
}


// parse

Node *new_node(Node_kind kind, Location* loc) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->loc = loc;
    return node;
}

Node *new_op(Node_kind kind, Node *lhs, Node *rhs, Location *loc) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    node->loc = loc;
    return node;
}

Node *new_node_num(int v, Location *loc) {
    Node *node = new_node(ND_NUM, loc);
    node->type = type_int;
    node->kind = ND_NUM;
    node->val = v;
    return node;
}

void toplevel();
Func *func(Type *ty, Token *name);
Type *type();
void type_array(Type**);
Vec *params();
Node *stmt();
Vec *block();

Node *rhs_expr();
Node *array(Location *start);

Node *expr();
Node *logical_or();
Node *logical_and();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *term();

Vec *args();

// initializer

Node *find_var(Token*);
Node *push_lvar(Type*, Token*);

void parse() {
    int len = vec_len(tokens);
    while (index < len)
        toplevel();
}

void toplevel() {
    Type *ty = type();
    Token *tk = expect(TK_IDT);
    if (consume_keyword("(")) {
        locals = vec_new();
        Func *f = func(ty, tk);
        vec_push(environment->functions, f);
    } else {
        Node *node = new_node(ND_GVAR, tk->loc);
        node->name = tk->str;
        node->len = strlen(tk->str);

        while (consume_keyword("[")) {
            Token *len = expect(TK_NUM);
            expect_keyword("]");
            ty = array_of(ty, len->val);
        }

        node->rhs = consume_keyword("=") ? rhs_expr() : NULL;

        expect_keyword(";");

        node->type = ty;
        vec_push(environment->globals, node);
    }
}

Func *func(Type *ty, Token *name) {
    Vec *par = params();
    Vec *blk = block();

    Func *func = calloc(1, sizeof(Func));
    func->name = name->str;
    func->params = par;
    func->block = blk;
    func->loc = name->loc;

    func->ret_type = ty;
    return func;
}

Type *type() {
    Type *typ = consume_type();
    if (typ == NULL)
        error_loc(lookahead_any()->loc, "unknown type");

    while (consume_keyword("*"))
        typ = ptr_of(typ);
    return typ;
}

Vec *params() {
    Vec *vec = vec_new();
    if (consume_keyword(")"))
        return vec;

    Type *ty = type();
    Token *tk = expect(TK_IDT);
    Node *param_0 = push_lvar(ty, tk);
    vec_push(vec, param_0);
    while (!consume_keyword(")")) {
        expect_keyword(",");
        Type *ty = type();
        Token *tk = expect(TK_IDT);
        Node *param_i = push_lvar(ty, tk);
        vec_push(vec, param_i);
    }
    return vec;
}

Vec *block() {
    int revert_locals_len = vec_len(locals);

    Vec *vec = vec_new();
    expect_keyword("{");
    while (!consume_keyword("}"))
        vec_push(vec, stmt());

    while (vec_len(locals) > revert_locals_len)
        vec_pop(locals);

    return vec;
}

Node *stmt() {
    Token *tk;
    Node *node;
    if ((tk = consume(TK_RETURN))) {
        node = new_op(ND_RETURN, expr(), NULL, tk->loc);
        expect_keyword(";");
    } else if ((tk = consume(TK_IF))) {
        node = new_node(ND_IF, tk->loc);
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->lhs = stmt();
        node->rhs = consume(TK_ELSE) ? stmt() : NULL;
    } else if ((tk = consume(TK_WHILE))) {
        node = new_node(ND_WHILE, tk->loc);
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->body = stmt();
    } else if ((tk = consume(TK_FOR))) {
        node = new_node(ND_FOR, tk->loc);
        expect_keyword("(");
        if (!consume_keyword(";")) {
            node->lhs = expr();
            expect_keyword(";");
        }
        if (!consume_keyword(";")) {
            node->cond = expr();
            expect_keyword(";");
        }
        if (!consume_keyword(")")) {
            node->rhs = expr();
            expect_keyword(")");
        }
        node->body = stmt();
    } else if (lookahead_keyword("{")) {
        Vec *vec = block();
        node = new_node(ND_BLOCK, NULL);
        node->block = vec;
    } else if (lookahead_any_type()) {
        Type *typ = type();
        Token *id = expect(TK_IDT);
        type_array(&typ);
        Node *rhs = consume_keyword("=") ? rhs_expr() : NULL;
        expect_keyword(";");

        Node *lhs = find_var(id);
        if (lhs == NULL)
            lhs = push_lvar(typ, id);

        node = new_op(ND_VARDECL, lhs, rhs, id->loc);
    } else {
        node = expr();
        expect_keyword(";");
    }
    return node;
}

void type_array(Type **t) {
    while (consume_keyword("[")) {
        Token *len = consume(TK_NUM);
        *t = array_of(*t, len->val);
        expect_keyword("]");
    }
}

Node *rhs_expr() {
    Token *tk = NULL;
    if ((tk = consume_keyword("{")))
        return array(tk->loc);

    return expr();
}

Node *array(Location *start) {
    Node *ret = new_node(ND_ARRAY, start);
    ret->block = vec_new();
    if (consume_keyword("}"))
        return ret;

    vec_push(ret->block, expr());

    while (!consume_keyword("}")) {
        expect_keyword(",");
        vec_push(ret->block, expr());
    }
    return ret;
}

Node *expr() {
    Node *node = logical_or();
    Token *tk;
    if ((tk = consume_keyword("=")))
        node = new_op(ND_ASGN, node, expr(), tk->loc);
    return node;
}

Node *logical_or() {
    Node *node = logical_and();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("||")))
            node = new_op(ND_LOR, node, logical_and(), tk->loc);
        else break;
    }
    return node;
}

Node *logical_and() {
    Node *node = equality();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("&&")))
            node = new_op(ND_LAND, node, equality(), tk->loc);
        else break;
    }
    return node;
}

Node *equality() {
    Node *node = relational();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("==")))
            node = new_op(ND_EQ, node, relational(), tk->loc);
        else if ((tk = consume_keyword("!=")))
            node = new_op(ND_NEQ, node, relational(), tk->loc);
        else break;
    }
    return node;
}

Node *relational() {
    Node *node = add();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("<")))
            node = new_op(ND_LT, node, add(), tk->loc);
        else if ((tk = consume_keyword("<=")))
            node = new_op(ND_LTE, node, add(), tk->loc);
        else if ((tk = consume_keyword(">")))
            node = new_op(ND_LT, add(), node, tk->loc);
        else if ((tk = consume_keyword(">=")))
            node = new_op(ND_LTE, add(), node, tk->loc);
        else break;
    }
    return node;
}

Node *add() {
    Node *node = mul();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("+")))
            node = new_op(ND_ADD, node, mul(), tk->loc);
        else if ((tk = consume_keyword("-")))
            node = new_op(ND_SUB, node, mul(), tk->loc);
        else break;
    }
    return node;
}

Node *mul() {
    Node *node = unary();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("*")))
            node = new_op(ND_MUL, node, unary(), tk->loc);
        else if ((tk = consume_keyword("/")))
            node = new_op(ND_DIV, node, unary(), tk->loc);
        else break;
    }
    return node;
}

Node *unary() {
    Token *tk;
    if ((tk = consume_keyword("sizeof")))
        return new_op(ND_SIZEOF, unary(), NULL, tk->loc);
    if (consume_keyword("+"))
        return term();
    if ((tk = consume_keyword("-")))
        return new_op(ND_SUB, new_node_num(0, NULL), term(), tk->loc);
    if ((tk = consume_keyword("&")))
        return new_op(ND_ADDR, unary(), NULL, tk->loc);
    if ((tk = consume_keyword("*")))
        return new_op(ND_DEREF, unary(), NULL, tk->loc);
    if ((tk = consume_keyword("!")))
        return new_op(ND_NEG, unary(), NULL, tk->loc);
    return term();
}

Node *term() {
    Token *tk = NULL;
    if ((tk = consume_keyword("("))) {
        Node *node = expr();
        if (!consume_keyword(")"))
            error_loc(tk->loc, ") couldn't be found");
        return node;
    }

    Node *node = NULL;
    if ((tk = consume(TK_IDT))) {
        if (consume_keyword("(")) { // CALL
            Vec *vec = args();

            node = new_node(ND_CALL, tk->loc);
            node->name = tk->str;
            node->len = strlen(tk->str);
            node->block = vec;
        } else { // variable
            node = find_var(tk);
            if (node == NULL) {
                error_loc(tk->loc, "variable not defined");
            }
        }
    } else if ((tk = consume(TK_NUM))) { // number
        node = new_node_num(tk->val, tk->loc);
    } else if ((tk = consume(TK_STRING))) {
        node = new_node(ND_STRING, tk->loc);
        node->name = tk->str;
        node->len = strlen(tk->str);
        node->type = type_ptr_char;
    }

    if (node == NULL) {
        if (tk == NULL)
            tk = lookahead_any();
        error_loc(tk->loc, "invalid expression or statement");
    }

    while ((tk = consume_keyword("["))) {
        Node *index_node = expr();
        expect_keyword("]");

        Node *add_node = new_op(ND_ADD, node, index_node, tk->loc);

        Node *next_node = new_node(ND_DEREF, tk->loc);
        next_node->lhs = add_node;

        node = next_node;
    }

    return node;
}

Vec *args() {
    Vec *vec = vec_new();
    if (consume_keyword(")"))
        return vec;
    vec_push(vec, expr());
    while (!consume_keyword(")")) {
        expect_keyword(",");
        vec_push(vec, expr());
    }
    return vec;
}

Node *find_var(Token *token) {
    if (token->kind != TK_IDT)
        error("find_lvar: lvar expected\n");

    int locals_len = locals == NULL ? 0 : vec_len(locals);
    for (int i = 0; i < locals_len; i++) {
        Node *var = vec_at(locals, i);
        if (strlen(token->str) == var->len && !strcmp(token->str, var->name))
            return var;
    }

    int globals_len = vec_len(environment->globals);
    for (int i = 0; i < globals_len; i++) {
        Node *var = vec_at(environment->globals, i);
        if (strlen(token->str) == var->len && !strcmp(token->str, var->name))
            return var;
    }

    return NULL;
}

Node *push_lvar(Type *ty, Token *tk) {
    Node *var = new_node(ND_LVAR, NULL);
    var->name = tk->str;
    var->type = ty;
    var->len = strlen(tk->str);

    vec_push(locals, var);
    return var;
}
