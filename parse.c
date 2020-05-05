#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

int index = 0;

void toplevel();
Func *func(Type *typ, Token *name);
Node *parse_struct(Node_kind kind, Location *loc);
Type *struct_pre(Location *loc);
Node *var_decl();
Type *type();
void type_array(Type**);
void type_pointer(Type**);
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

// Parse helpers

Token *lookahead_any();
Token *lookahead(Token_kind kind);
Token *lookahead_keyword(char *str);
Token *consume(Token_kind kind);
Token *consume_keyword(char *str);
Token *consume_identifier();
Token *expect(Token_kind kind);
Token *expect_keyword(char *str);
Token *lookahead_type(char *str);
bool lookahead_var_decl();
Type *consume_type_identifier();
Type *consume_type_pre();

// Node helpers

Node *mknode(Node_kind kind, Location *loc);
Node *mkop(Node_kind kind, Node *lhs, Node *rhs, Location *loc);
Node *mknum(int v, Location *loc);

// Environment helpers

Node *find_var(Token* token);
Node *push_lvar(Type* typ, Token* token);
Type *find_struct(char *name);
Type *lookup_alias(char *alias);

// Parse

void parse() {
    int len = vec_len(tokens);
    while (index < len)
        toplevel();
}

void toplevel() {
    Token *tk;
    if ((tk = consume_keyword("typedef"))) {
        Type *typ = consume_keyword("struct")
            ? struct_pre(tk->loc)
            : consume_type_pre();

        if (typ == NULL)
            error_loc(tk->loc, "[parse] type expected");

        Token *id = expect(TK_IDT);
        expect_keyword(";");

        Alias *alias = calloc(1, sizeof(Alias));
        alias->name = id->str;
        alias->type = typ;
        vec_push(environment->aliases, alias);
        return;
    }
    if ((tk = consume_keyword("struct"))) {
        locals = vec_new();
        Node *gvar = parse_struct(ND_GVAR, tk->loc);

        if (gvar != NULL) {
            type_array(&gvar->type);
            gvar->rhs = consume_keyword("=") ? rhs_expr() : NULL;
            vec_push(environment->globals, gvar);
        }
        expect_keyword(";");
        return;
    }

    Type *typ = type();
    tk = expect(TK_IDT);
    if (consume_keyword("(")) {
        locals = vec_new();
        Func *f = func(typ, tk);
        vec_push(environment->functions, f);
    } else {
        Node *node = mknode(ND_GVAR, tk->loc);
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
}

Func *func(Type *typ, Token *name) {
    Vec *par = params();
    Vec *blk = block();

    Func *func = calloc(1, sizeof(Func));
    func->name = name->str;
    func->params = par;
    func->block = blk;
    func->loc = name->loc;

    func->ret_type = typ;
    return func;
}

Type *struct_pre(Location *start) {
    Token *strc_id = consume(TK_IDT);
    Vec *fields = NULL;
    if (consume_keyword("{")) {
        fields = vec_new();
        while (!consume_keyword("}")) {
            vec_push(fields, var_decl());
            expect_keyword(";");
        }
    }

    if (strc_id == NULL && fields == NULL)
        error_loc(start, "[parse] invalid struct statement");

    Struct *strc = calloc(1, sizeof(Struct));
    strc->name = (strc_id == NULL) ? NULL : strc_id->str;
    strc->fields = fields;
    strc->loc = start;

    Type *typ;
    if (strc->fields == NULL)
        typ = find_struct(strc->name);
    else {
        typ = calloc(1, sizeof(Type));
        typ->ty = TY_STRUCT;
        typ->strct = strc;
    }

    if (typ != NULL && typ->strct->name != NULL)
        vec_push(environment->structs, typ->strct);

    type_pointer(&typ);
    return typ;
}

// 'struct' . [struct_id]? ('{' [var_decl]* '}')? [ident]?
Node *parse_struct(Node_kind kind, Location *start) {
    Type *typ = struct_pre(start);

    Token *id = consume(TK_IDT);

    if (typ == NULL || (id == NULL && typ->ty == TY_PTR))
        error_loc(start, "[parse] invalid struct declaration");

    if (id == NULL)
        return NULL;

    Node *ret = mknode(kind, start);
    ret->name = id->str;
    ret->type = typ;
    ret->len = strlen(id->str);
    return ret;
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
    int revert_structs_len = vec_len(environment->structs);

    Vec *vec = vec_new();
    expect_keyword("{");
    while (!consume_keyword("}"))
        vec_push(vec, stmt());

    while (vec_len(locals) > revert_locals_len)
        vec_pop(locals);
    while (vec_len(environment->structs) > revert_structs_len)
        vec_pop(environment->structs);

    return vec;
}

Node *stmt() {
    Token *tk;
    Node *node;
    if ((tk = consume_keyword("return"))) {
        node = mkop(ND_RETURN, expr(), NULL, tk->loc);
        expect_keyword(";");
    } else if ((tk = consume_keyword("if"))) {
        node = mknode(ND_IF, tk->loc);
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->lhs = stmt();
        node->rhs = consume_keyword("else") ? stmt() : NULL;
    } else if ((tk = consume_keyword("while"))) {
        node = mknode(ND_WHILE, tk->loc);
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->body = stmt();
    } else if ((tk = consume_keyword("for"))) {
        int revert_locals_len = vec_len(locals);
        node = mknode(ND_FOR, tk->loc);
        expect_keyword("(");
        if (!consume_keyword(";")) {
            if (lookahead_var_decl()) {
                Node *var = var_decl();
                Node *e = consume_keyword("=") ? rhs_expr() : NULL;
                node->lhs = mkop(ND_VARDECL, var, e, var->loc);
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
        node = mknode(ND_BLOCK, NULL);
        node->block = vec;
    } else if (lookahead_var_decl()) {
        Node *lhs = var_decl();
        Node *rhs = consume_keyword("=") ? rhs_expr() : NULL;
        expect_keyword(";");

        node = mkop(ND_VARDECL, lhs, rhs, lhs->loc);
    } else {
        node = expr();
        expect_keyword(";");
    }
    return node;
}

void type_pointer(Type **t) {
    while (consume_keyword("*"))
        *t = ptr_of(*t);
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
    Node *ret = mknode(ND_ARRAY, start);
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
        node = mkop(ND_SEQ, node, assignment(), tk->loc);
    return node;
}

Node *assignment() {
    Node *node = conditional();
    Token *tk;
    if ((tk = consume_keyword("=")))
        node = mkop(ND_ASGN, node, expr(), tk->loc);
    return node;
}

Node *conditional() {
    Node *cond = logical_or();
    if (!consume_keyword("?"))
        return cond;
    Node *true_expr = expr();
    expect_keyword(":");
    Node *false_expr = conditional();

    Node *ret = mkop(ND_COND, true_expr, false_expr, cond->loc);
    ret->cond = cond;
    return ret;
}

Node *logical_or() {
    Node *node = logical_and();
    for (Token *tk; (tk = consume_keyword("||"));)
        node = mkop(ND_LOR, node, logical_and(), tk->loc);
    return node;
}

Node *logical_and() {
    Node *node = inclusive_or();
    for (Token *tk; (tk = consume_keyword("&&"));)
        node = mkop(ND_LAND, node, inclusive_or(), tk->loc);
    return node;
}

Node *inclusive_or() {
    Node *node = exclusive_or();
    for (Token *tk; (tk = consume_keyword("|"));)
        node = mkop(ND_IOR, node, exclusive_or(), tk->loc);
    return node;
}

Node *exclusive_or() {
    Node *node = and();
    for (Token *tk; (tk = consume_keyword("^"));)
        node = mkop(ND_XOR, node, and(), tk->loc);
    return node;
}

Node *and() {
    Node *node = equality();
    for (Token *tk; (tk = consume_keyword("&"));)
        node = mkop(ND_AND, node, equality(), tk->loc);
    return node;
}

Node *equality() {
    Node *node = relational();
    for (Token *tk;;) {
        if ((tk = consume_keyword("==")))
            node = mkop(ND_EQ, node, relational(), tk->loc);
        else if ((tk = consume_keyword("!=")))
            node = mkop(ND_NEQ, node, relational(), tk->loc);
        else break;
    }
    return node;
}

Node *relational() {
    Node *node = shift();
    for (Token *tk;;) {
        if ((tk = consume_keyword("<")))
            node = mkop(ND_LT, node, shift(), tk->loc);
        else if ((tk = consume_keyword("<=")))
            node = mkop(ND_LTE, node, shift(), tk->loc);
        else if ((tk = consume_keyword(">")))
            node = mkop(ND_LT, shift(), node, tk->loc);
        else if ((tk = consume_keyword(">=")))
            node = mkop(ND_LTE, shift(), node, tk->loc);
        else break;
    }
    return node;
}

Node *shift() {
    Node *node = add();
    for (Token *tk;;) {
        if ((tk = consume_keyword("<<")))
            node = mkop(ND_LSH, node, add(), tk->loc);
        else if ((tk = consume_keyword(">>")))
            node = mkop(ND_RSH, node, add(), tk->loc);
        else break;
    }
    return node;
}

Node *add() {
    Node *node = mul();
    for (Token *tk;;) {
        if ((tk = consume_keyword("+")))
            node = mkop(ND_ADD, node, mul(), tk->loc);
        else if ((tk = consume_keyword("-")))
            node = mkop(ND_SUB, node, mul(), tk->loc);
        else break;
    }
    return node;
}

Node *mul() {
    Node *node = unary();
    for (Token *tk;;) {
        if ((tk = consume_keyword("*")))
            node = mkop(ND_MUL, node, unary(), tk->loc);
        else if ((tk = consume_keyword("/")))
            node = mkop(ND_DIV, node, unary(), tk->loc);
        else if ((tk = consume_keyword("%")))
            node = mkop(ND_MOD, node, unary(), tk->loc);
        else break;
    }
    return node;
}

Node *unary() {
    Token *tk;
    return (tk = consume_keyword("sizeof")) ? mkop(ND_SIZEOF, unary(), NULL, tk->loc)
         : consume_keyword("+")             ? term()
         : (tk = consume_keyword("-"))      ? mkop(ND_SUB, mknum(0, tk->loc), term(), tk->loc)
         : (tk = consume_keyword("&"))      ? mkop(ND_ADDR, unary(), NULL, tk->loc)
         : (tk = consume_keyword("*"))      ? mkop(ND_DEREF, unary(), NULL, tk->loc)
         : (tk = consume_keyword("!"))      ? mkop(ND_NEG, unary(), NULL, tk->loc)
         : term();
}

Node *term() {
    Token *tk = NULL;
    Node *node = NULL;
    if ((tk = consume_keyword("("))) {
        node = expr();
        if (!consume_keyword(")"))
            error_loc(tk->loc, "no matching parenthesis");
    }

    if ((tk = consume(TK_IDT))) {
        if (consume_keyword("(")) { // CALL
            node = mknode(ND_CALL, tk->loc);
            node->name = tk->str;
            node->len = strlen(tk->str);
            node->block = args();
        } else { // variable
            node = find_var(tk);
            if (node == NULL)
                error_loc(tk->loc, "variable not defined");
        }
    } else if ((tk = consume(TK_NUM))) { // number
        node = mknum(tk->val, tk->loc);
    } else if ((tk = consume(TK_STRING))) {
        node = mknode(ND_STRING, tk->loc);
        node->name = tk->str;
        node->len = strlen(tk->str);
        node->type = type_ptr_char;
    } else if ((tk = consume(TK_CHAR))) {
        node = mknode(ND_CHAR, tk->loc);
        node->val = tk->val;
        node->type = type_char;
    }

    if (node == NULL) {
        tk = lookahead_any();
        error_loc(tk->loc, "invalid expression or statement");
    }

    for (Token *tk;;) {
        if ((tk = consume_keyword("["))) {
            Node *index_node = expr();
            expect_keyword("]");

            Node *add_node = mkop(ND_ADD, node, index_node, tk->loc);

            Node *next_node = mknode(ND_DEREF, tk->loc);
            next_node->lhs = add_node;

            node = next_node;
        } else if ((tk = consume_keyword("."))) {
            Token *attr = expect(TK_IDT);
            Node *attr_node = mkop(ND_ATTR, node, NULL, tk->loc);
            attr_node->attr = attr;
            node = attr_node;
        } else if ((tk = consume_keyword("->"))) {
            Token *attr = expect(TK_IDT);
            Node *l = mkop(ND_DEREF, node, NULL, tk->loc);
            Node *next_node = mkop(ND_ATTR, l, NULL, tk->loc);
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

// Parse helpers

Token *lookahead_any() {
    if (index >= vec_len(tokens))
        return NULL;
    return vec_at(tokens, index);
}

Token *lookahead(Token_kind kind) {
    Token *tk = lookahead_any();
    return (tk != NULL && tk->kind == kind) ? tk : NULL;
}

Token *lookahead_keyword(char *str) {
    Token *tk = lookahead(TK_KWD);
    return (tk != NULL && !strcmp(str, tk->str)) ? tk : NULL;
}

Token *consume(Token_kind kind) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != kind)
        return NULL;
    index++;
    return tk;
}

Token *consume_keyword(char *str) {
    Token *tk = lookahead(TK_KWD);
    if (tk == NULL || strcmp(str, tk->str))
        return NULL;
    index++;
    return tk;
}

Token *consume_identifier() {
    Token *tk = lookahead(TK_IDT);
    Type *typ = lookup_alias(tk->str);
    if (typ == NULL) {
        index++;
        return tk;
    }
    return NULL;
} 

Token *expect(Token_kind kind) {
    Token *tk = lookahead_any();
    if (tk->kind != kind)
        error_loc(tk->loc, "%s expected\n", kind);
    index++;
    return tk;
}

Token *expect_keyword(char* str) {
    Token *tk = lookahead_any();
    if (tk->kind != TK_KWD || strcmp(str, tk->str))
        error_loc(tk->loc, "\"%s\" expected", str);
    index++;
    return tk;
}

Token *lookahead_type(char* str) {
    Token *tk = lookahead(TK_IDT);
    return (tk != NULL && !strcmp(str, tk->str)) ? tk : NULL;
}

bool lookahead_var_decl() {
    Token *tk;
    return lookahead_keyword("struct") != NULL
        || lookahead_type("int") != NULL
        || lookahead_type("char")
        || (((tk = lookahead(TK_IDT)) != NULL)
                && lookup_alias(lookahead(TK_IDT)->str) != NULL);
}

Type *consume_type_identifier() {
    if (lookahead_type("int")) {
        index++;
        return type_int;
    } else if (lookahead_type("char")) {
        index++;
        return type_char;
    }

    Token *id = lookahead(TK_IDT);
    Type *typ = lookup_alias(id->str);
    if (typ != NULL)
        index++;
    return typ;
}

Type *consume_type_pre() {
    Token *tk;
    Type *typ;
    if ((tk = consume_keyword("struct"))) {
        Token *strc_id = expect(TK_IDT);
        typ = find_struct(strc_id->str);
    } else
        typ = consume_type_identifier();

    if (typ == NULL)
        return NULL;

    type_pointer(&typ);
    return typ;
}

// Node helpers

Node *mknode(Node_kind kind, Location* loc) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->loc = loc;
    return node;
}

Node *mkop(Node_kind kind, Node *lhs, Node *rhs, Location *loc) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    node->loc = loc;
    return node;
}

Node *mknum(int v, Location *loc) {
    Node *node = mknode(ND_NUM, loc);
    node->type = type_int;
    node->kind = ND_NUM;
    node->val = v;
    return node;
}


// Environment helpers

Node *find_lvar(Token *token) {
    int locals_len = locals == NULL ? 0 : vec_len(locals);
    for (int i = 0; i < locals_len; i++) {
        Node *var = vec_at(locals, i);
        if (!strcmp(token->str, var->name))
            return var;
    }
    return NULL;
}

Node *find_gvar(Token *token) {
    int globals_len = vec_len(environment->globals);
    for (int i = 0; i < globals_len; i++) {
        Node *var = vec_at(environment->globals, i);
        if (!strcmp(token->str, var->name))
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

Node *push_lvar(Type *typ, Token *tk) {
    if (find_lvar(tk) != NULL)
        error_loc(tk->loc, "[parse] duplicate variable");

    Node *var = mknode(ND_LVAR, tk->loc);
    var->name = tk->str;
    var->type = typ;
    var->len = strlen(tk->str);

    vec_push(locals, var);
    return var;
}

Type *find_struct(char *name) {
    int len = vec_len(environment->structs);
    for (int i = 0; i < len; i++) {
        Struct *strc = vec_at(environment->structs, i);
        if (!strcmp(strc->name, name)) {
            Type *typ = calloc(1, sizeof(Type));
            typ->ty = TY_STRUCT;
            typ->strct = strc;
            return typ;
        }
    }
    return NULL;
}

Type *lookup_alias(char *str) {
    int len = vec_len(environment->aliases);
    for (int i = 0; i < len; i++) {
        Alias *al = vec_at(environment->aliases, i);
        if (!strcmp(al->name, str))
            return al->type;
    }
    return NULL;
}
