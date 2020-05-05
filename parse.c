#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

void toplevel();
Func *func(Type *ty, Token *name);
Struct *struct_decl(Token *id);
Node *var_decl();
Type *type();
void type_array(Type**);
Vec *params();
Node *stmt();
Vec *block();

Node *rhs_expr();
Node *array(Location *start);

Node *expr();
Node *assignment();
Node *conditional();
Node *logical_or();
Node *logical_and();
Node *inclusive_or();
Node *exclusive_or();
Node *and();
Node *equality();
Node *relational();
Node *shift();
Node *add();
Node *mul();
Node *unary();
Node *term();

Vec *args();

Node *find_var(Token*);
Node *push_lvar(Type*, Token*);
Type *find_struct(char *name);
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

Token *expect_keyword(char* str) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != TK_KWD || strlen(str) != strlen(tk->str) || strcmp(str, tk->str))
        error_loc(tk->loc, "\"%s\" expected", str);
    index++;
    return tk;
}

bool lookahead_type(char* str) {
    Token *tk = lookahead(TK_IDT);
    return tk != NULL && strlen(str) == strlen(tk->str) && !strcmp(str, tk->str);
}

bool lookahead_var_decl() {
    return lookahead_keyword("struct") || lookahead_type("int") || lookahead_type("char");
}

Type *consume_type_identifier() {
    if (lookahead_type("int")) {
        index++;
        return type_int;
    } else if (lookahead_type("char")) {
        index++;
        return type_char;
    }
    return NULL;
}

Type *consume_type_pre() {
    Type *typ;
    if (consume_keyword("struct")) {
        Token *strc_id = expect(TK_IDT);
        typ = find_struct(strc_id->str);
    } else
        typ = consume_type_identifier();

    if (typ == NULL)
        return NULL;

    while (consume_keyword("*"))
        typ = ptr_of(typ);

    return typ;
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

void parse() {
    int len = vec_len(tokens);
    while (index < len)
        toplevel();
}

void toplevel() {
    if (consume_keyword("struct")) {
        Token *strc_id = expect(TK_IDT);
        if (lookahead_keyword("{")) {
            Struct *strc = struct_decl(strc_id);
            expect_keyword(";");
            vec_push(environment->structs, strc);
        } else {
            Type *typ = find_struct(strc_id->str);
            if (typ == NULL)
                error_loc(strc_id->loc, "undefined struct");

            Token *tk = expect(TK_IDT);

            Node *node = new_node(ND_GVAR, tk->loc);
            node->name = tk->str;
            node->len = strlen(tk->str);

            while (consume_keyword("[")) {
                Token *len = expect(TK_NUM);
                expect_keyword("]");
                typ = array_of(typ, len->val);
            }

            node->rhs = consume_keyword("=") ? rhs_expr() : NULL;

            expect_keyword(";");

            node->type = typ;
            vec_push(environment->globals, node);
        }
        return;
    }
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

Struct *struct_decl(Token *id) {
    Struct *s = calloc(1, sizeof(Struct));
    s->name = id->str;
    s->fields = vec_new();
    s->loc = id->loc;

    expect_keyword("{");

    locals = vec_new();
    while (!consume_keyword("}")) {
        vec_push(s->fields, var_decl());
        expect_keyword(";");
    }
    return s;
}

Node *var_decl() {
    Type *typ = type();
    Token *tk = expect(TK_IDT);
    type_array(&typ);
    return push_lvar(typ, tk);
}

Type *type() {
    Type *typ = consume_type_pre();
    if (typ == NULL)
        error_loc(lookahead_any()->loc, "unknown type");
    return typ;
}

Vec *params() {
    Vec *vec = vec_new();
    if (consume_keyword(")"))
        return vec;

    vec_push(vec, var_decl());
    while (!consume_keyword(")")) {
        expect_keyword(",");
        vec_push(vec, var_decl());
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
    if ((tk = consume_keyword("return"))) {
        node = new_op(ND_RETURN, expr(), NULL, tk->loc);
        expect_keyword(";");
    } else if ((tk = consume_keyword("if"))) {
        node = new_node(ND_IF, tk->loc);
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->lhs = stmt();
        node->rhs = consume_keyword("else") ? stmt() : NULL;
    } else if ((tk = consume_keyword("while"))) {
        node = new_node(ND_WHILE, tk->loc);
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->body = stmt();
    } else if ((tk = consume_keyword("for"))) {
        int revert_locals_len = vec_len(locals);
        node = new_node(ND_FOR, tk->loc);
        expect_keyword("(");
        if (!consume_keyword(";")) {
            if (lookahead_var_decl()) {
                Node *var = var_decl();
                Node *e = consume_keyword("=") ? rhs_expr() : NULL;
                node->lhs = new_op(ND_VARDECL, var, e, var->loc);
            } else
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
        while (vec_len(locals) > revert_locals_len) vec_pop(locals);
    } else if (lookahead_keyword("{")) {
        Vec *vec = block();
        node = new_node(ND_BLOCK, NULL);
        node->block = vec;
    } else if (lookahead_var_decl()) {
        Node *lhs = var_decl();
        Node *rhs = consume_keyword("=") ? rhs_expr() : NULL;
        expect_keyword(";");

        node = new_op(ND_VARDECL, lhs, rhs, lhs->loc);
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

    vec_push(ret->block, assignment());

    while (!consume_keyword("}")) {
        expect_keyword(",");
        vec_push(ret->block, assignment());
    }
    return ret;
}

Node *expr() {
    Node *node = assignment();
    for (Token *tk; (tk = consume_keyword(","));)
        node = new_op(ND_SEQ, node, assignment(), tk->loc);
    return node;
}

Node *assignment() {
    Node *node = conditional();
    Token *tk;
    if ((tk = consume_keyword("=")))
        node = new_op(ND_ASGN, node, expr(), tk->loc);
    return node;
}

Node *conditional() {
    Node *cond = logical_or();
    if (!consume_keyword("?"))
        return cond;
    Node *true_expr = expr();
    expect_keyword(":");
    Node *false_expr = conditional();

    Node *ret = new_op(ND_COND, true_expr, false_expr, cond->loc);
    ret->cond = cond;
    return ret;
}

Node *logical_or() {
    Node *node = logical_and();
    for (Token *tk; (tk = consume_keyword("||"));)
        node = new_op(ND_LOR, node, logical_and(), tk->loc);
    return node;
}

Node *logical_and() {
    Node *node = inclusive_or();
    for (Token *tk; (tk = consume_keyword("&&"));)
        node = new_op(ND_LAND, node, inclusive_or(), tk->loc);
    return node;
}

Node *inclusive_or() {
    Node *node = exclusive_or();
    for (Token *tk; (tk = consume_keyword("|"));)
        node = new_op(ND_IOR, node, exclusive_or(), tk->loc);
    return node;
}

Node *exclusive_or() {
    Node *node = and();
    for (Token *tk; (tk = consume_keyword("^"));)
        node = new_op(ND_XOR, node, and(), tk->loc);
    return node;
}

Node *and() {
    Node *node = equality();
    for (Token *tk; (tk = consume_keyword("&"));)
        node = new_op(ND_AND, node, equality(), tk->loc);
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
    Node *node = shift();
    for (;;) {
        Token *tk;
        if ((tk = consume_keyword("<")))
            node = new_op(ND_LT, node, shift(), tk->loc);
        else if ((tk = consume_keyword("<=")))
            node = new_op(ND_LTE, node, shift(), tk->loc);
        else if ((tk = consume_keyword(">")))
            node = new_op(ND_LT, shift(), node, tk->loc);
        else if ((tk = consume_keyword(">=")))
            node = new_op(ND_LTE, shift(), node, tk->loc);
        else break;
    }
    return node;
}

Node *shift() {
    Node *node = add();
    for (Token *tk;;) {
        if ((tk = consume_keyword("<<")))
            node = new_op(ND_LSH, node, add(), tk->loc);
        else if ((tk = consume_keyword(">>")))
            node = new_op(ND_RSH, node, add(), tk->loc);
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
        else if ((tk = consume_keyword("%")))
            node = new_op(ND_MOD, node, unary(), tk->loc);
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
    Node *node = NULL;
    if ((tk = consume_keyword("("))) {
        node = expr();
        if (!consume_keyword(")"))
            error_loc(tk->loc, ") couldn't be found");
    }

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
    } else if ((tk = consume(TK_CHAR))) {
        node = new_node(ND_CHAR, tk->loc);
        node->val = tk->val;
        node->type = type_char;
    }

    if (node == NULL) {
        if (tk == NULL)
            tk = lookahead_any();
        error_loc(tk->loc, "invalid expression or statement");
    }

    for (Token *tk;;) {
        if ((tk = consume_keyword("["))) {
            Node *index_node = expr();
            expect_keyword("]");

            Node *add_node = new_op(ND_ADD, node, index_node, tk->loc);

            Node *next_node = new_node(ND_DEREF, tk->loc);
            next_node->lhs = add_node;

            node = next_node;
        } else if ((tk = consume_keyword("."))) {
            Token *attr = expect(TK_IDT);
            Node *attr_node = new_op(ND_ATTR, node, NULL, tk->loc);
            attr_node->attr = attr;
            node = attr_node;
        } else if ((tk = consume_keyword("->"))) {
            Token *attr = expect(TK_IDT);
            Node *l = new_op(ND_DEREF, node, NULL, tk->loc);
            Node *next_node = new_op(ND_ATTR, l, NULL, tk->loc);
            next_node->attr = attr;
            node = next_node;
        } else
            break;
    }

    return node;
}

Vec *args() {
    Vec *vec = vec_new();
    if (consume_keyword(")"))
        return vec;
    vec_push(vec, assignment());
    while (!consume_keyword(")")) {
        expect_keyword(",");
        vec_push(vec, assignment());
    }
    return vec;
}

Node *find_lvar(Token *token) {
    int locals_len = locals == NULL ? 0 : vec_len(locals);
    for (int i = 0; i < locals_len; i++) {
        Node *var = vec_at(locals, i);
        if (strlen(token->str) == var->len && !strcmp(token->str, var->name))
            return var;
    }
    return NULL;
}

Node *find_gvar(Token *token) {
    int globals_len = vec_len(environment->globals);
    for (int i = 0; i < globals_len; i++) {
        Node *var = vec_at(environment->globals, i);
        if (strlen(token->str) == var->len && !strcmp(token->str, var->name))
            return var;
    }
    return NULL;
}

Node *find_var(Token *token) {
    if (token->kind != TK_IDT)
        error("find_var: variable expected\n");

    Node *lvar = find_lvar(token);
    if (lvar != NULL)
        return lvar;

    return find_gvar(token);
}

Node *push_lvar(Type *ty, Token *tk) {
    if (find_lvar(tk) != NULL)
        error_loc(tk->loc, "[parse] duplicate variable");

    Node *var = new_node(ND_LVAR, tk->loc);
    var->name = tk->str;
    var->type = ty;
    var->len = strlen(tk->str);

    vec_push(locals, var);
    return var;
}

Type *find_struct(char *name) {
    int len = vec_len(environment->structs);
    for (int i = 0; i < len; i++) {
        Struct *strc = vec_at(environment->structs, i);
        int slen = strlen(strc->name);
        if (strlen(name) == slen && !strncmp(strc->name, name, slen)) {
            Type *typ = calloc(1, sizeof(Type));
            typ->ty = TY_STRUCT;
            typ->strct = strc;
            return typ;
        }
    }
    return NULL;
}
