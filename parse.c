#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

int index = 0;
Vec *functions;
Map *global_vars;
Environment *structs;
Environment *builtin_aliases;
Environment *aliases;

void toplevel();
Node *parse_struct(Node_kind kind, Location *loc);
Type *struct_pre(Location *loc);
Node *var_decl();
Type *type();
Type *type_array(Type*);
Type *type_pointer(Type*);
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
Node *postfix();
Node *primary();
Vec *args();

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

Node *mknode(Node_kind kind, Location *loc);
Node *binop(Node_kind kind, Node *lhs, Node *rhs, Location *loc);
Node *mknum(int v, Location *loc);

// Parse

void parse() {
    structs = env_new(NULL);
    aliases = env_new(builtin_aliases);

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

        env_push(aliases, id->str, typ);
        return;
    }
    if ((tk = consume_keyword("struct"))) {
        Node *gvar = parse_struct(ND_GVAR, tk->loc);

        if (gvar != NULL) {
            gvar->type = type_array(gvar->type);
            gvar->rhs = consume_keyword("=") ? rhs_expr() : NULL;
            map_put(global_vars, gvar->name, gvar);
        }
        expect_keyword(";");
        return;
    }

    bool is_extern = consume_keyword("extern");

    Type *typ = type();
    tk = expect(TK_IDT);
    if (consume_keyword("(")) {
        Func *func = calloc(1, sizeof(Func));
        func->name = tk->str;
        func->loc = tk->loc;
        func->params = params();
        func->ret_type = typ;
        if (consume_keyword(";")) {
            func->is_extern = true;
        } else {
            func->is_extern = false;
            Vec *blk = block();
            if (is_extern)
                error_loc(tk->loc, "[parse] prototype function declaration shouldn't have a body");
            func->block = blk;
            func->global_vars = map_new();
            for (int i = 0; i < vec_len(global_vars->values); i++) {
                Node *gvar = vec_at(global_vars->values, i);
                map_put(func->global_vars, gvar->name, gvar);
            }
        }
        vec_push(functions, func);
    } else {
        Node *node = mknode(ND_GVAR, tk->loc);
        node->name = tk->str;
        node->is_extern = is_extern;

        typ = type_array(typ);

        if (consume_keyword("=")) {
            if (is_extern)
                error_loc(tk->loc, "[parse] extern variable declaration shouldn't have a value");
            node->rhs = rhs_expr();
        } else
            node->rhs = NULL;

        expect_keyword(";");

        node->type = typ;
        map_put(global_vars, node->name, node);
    }
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
        typ = env_find(structs, strc->name);
    else {
        typ = calloc(1, sizeof(Type));
        typ->ty = TY_STRUCT;
        typ->strct = strc;
    }

    if (typ != NULL && typ->strct->name != NULL)
        env_push(structs, typ->strct->name, typ);

    typ = type_pointer(typ);
    return typ;
}

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
    return ret;
}

Node *var_decl() {
    Type *typ = type();
    Token *tk = expect(TK_IDT);

    typ = type_array(typ);
    Node *var = mknode(ND_VAR, tk->loc);
    var->name = tk->str;
    var->type = typ;
    return var;
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
    structs = env_new(structs);

    Vec *vec = vec_new();
    expect_keyword("{");
    while (!consume_keyword("}"))
        vec_push(vec, stmt());

    structs = env_next(structs);
    return vec;
}

Node *stmt() {
    Token *tk;
    Node *node;
    if ((tk = consume_keyword("return"))) {
        Node *ret = NULL;
        if (consume_keyword(";") == NULL) {
            ret = expr();
            expect_keyword(";");
        }
        node = binop(ND_RETURN, ret, NULL, tk->loc);
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
        node = mknode(ND_FOR, tk->loc);
        expect_keyword("(");
        if (!consume_keyword(";")) {
            if (lookahead_var_decl()) {
                Node *var = var_decl();
                Node *e = consume_keyword("=") ? rhs_expr() : NULL;
                node->lhs = binop(ND_VARDECL, var, e, var->loc);
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
    } else if ((tk = consume_keyword("do"))) {
        node = mknode(ND_DOWHILE, tk->loc);
        node->body = stmt();
        expect_keyword("while");
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        expect_keyword(";");
    } else if ((tk = consume_keyword("break"))) {
        expect_keyword(";");
        node = mknode(ND_BREAK, tk->loc);
    } else if ((tk = consume_keyword("continue"))) {
        expect_keyword(";");
        node = mknode(ND_CONTINUE, tk->loc);
    } else if ((tk = consume_keyword("switch"))) {
        node = mknode(ND_SWITCH, tk->loc);
        expect_keyword("(");
        node->cond = expr();
        expect_keyword(")");
        node->block = block();
    } else if ((tk = consume_keyword("case"))) {
        node = mknode(ND_CASE, tk->loc);
        node->lhs = expr();
        expect_keyword(":");
    } else if ((tk = consume_keyword("default"))) {
        node = mknode(ND_DEFAULT, tk->loc);
        expect_keyword(":");
    } else if ((tk = lookahead_keyword("{"))) {
        Vec *vec = block();
        node = mknode(ND_BLOCK, tk->loc);
        node->block = vec;
    } else if (lookahead_var_decl()) {
        Node *lhs = var_decl();
        Node *rhs = consume_keyword("=") ? rhs_expr() : NULL;
        node = binop(ND_VARDECL, lhs, rhs, lhs->loc);
        expect_keyword(";");
    } else {
        node = expr();
        expect_keyword(";");
    }
    return node;
}

Type *type_pointer(Type *t) {
    while (consume_keyword("*")) t = ptr_of(t);
    return t;
}

Type *type_array(Type *t) {
    while (consume_keyword("[")) {
        Token *len = consume(TK_NUM);
        t = array_of(t, len->val);
        expect_keyword("]");
    }
    return t;
}

// Expressions

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
        node = binop(ND_SEQ, node, assignment(), tk->loc);
    return node;
}

// TODO: assignment ::= conditional | unary unary-op assignment
Node *assignment() {
    Node *node = conditional();
    Token *tk;
    if ((tk = consume_keyword("=")))
        return binop(ND_ASGN, node, assignment(), tk->loc);
    if ((tk = consume_keyword("+=")))
        return binop(ND_ADDEQ, node, assignment(), tk->loc);
    if ((tk = consume_keyword("-=")))
        return binop(ND_SUBEQ, node, assignment(), tk->loc);
    if ((tk = consume_keyword("*=")))
        return binop(ND_MULEQ, node, assignment(), tk->loc);
    if ((tk = consume_keyword("/=")))
        return binop(ND_DIVEQ, node, assignment(), tk->loc);
    if ((tk = consume_keyword("%=")))
        return binop(ND_MODEQ, node, assignment(), tk->loc);
    if ((tk = consume_keyword("<<=")))
        return binop(ND_LSHEQ, node, assignment(), tk->loc);
    if ((tk = consume_keyword(">>=")))
        return binop(ND_RSHEQ, node, assignment(), tk->loc);
    if ((tk = consume_keyword("&=")))
        return binop(ND_ANDEQ, node, assignment(), tk->loc);
    if ((tk = consume_keyword("|=")))
        return binop(ND_IOREQ, node, assignment(), tk->loc);
    if ((tk = consume_keyword("^=")))
        return binop(ND_XOREQ, node, assignment(), tk->loc);

    return node;
}

Node *conditional() {
    Node *cond = logical_or();
    if (!consume_keyword("?"))
        return cond;

    Node *ret = mknode(ND_COND, cond->loc);
    ret->cond = cond;
    ret->lhs = expr();
    expect_keyword(":");
    ret->rhs = conditional();
    return ret;
}

Node *logical_or() {
    Node *node = logical_and();
    for (Token *tk; (tk = consume_keyword("||"));)
        node = binop(ND_LOR, node, logical_and(), tk->loc);
    return node;
}

Node *logical_and() {
    Node *node = inclusive_or();
    for (Token *tk; (tk = consume_keyword("&&"));)
        node = binop(ND_LAND, node, inclusive_or(), tk->loc);
    return node;
}

Node *inclusive_or() {
    Node *node = exclusive_or();
    for (Token *tk; (tk = consume_keyword("|"));)
        node = binop(ND_IOR, node, exclusive_or(), tk->loc);
    return node;
}

Node *exclusive_or() {
    Node *node = and();
    for (Token *tk; (tk = consume_keyword("^"));)
        node = binop(ND_XOR, node, and(), tk->loc);
    return node;
}

Node *and() {
    Node *node = equality();
    for (Token *tk; (tk = consume_keyword("&"));)
        node = binop(ND_AND, node, equality(), tk->loc);
    return node;
}

Node *equality() {
    Node *node = relational();
    for (Token *tk;;) {
        if ((tk = consume_keyword("==")))
            node = binop(ND_EQ, node, relational(), tk->loc);
        else if ((tk = consume_keyword("!=")))
            node = binop(ND_NEQ, node, relational(), tk->loc);
        else break;
    }
    return node;
}

Node *relational() {
    Node *node = shift();
    for (Token *tk;;) {
        if ((tk = consume_keyword("<")))
            node = binop(ND_LT, node, shift(), tk->loc);
        else if ((tk = consume_keyword("<=")))
            node = binop(ND_LTE, node, shift(), tk->loc);
        else if ((tk = consume_keyword(">")))
            node = binop(ND_LT, shift(), node, tk->loc);
        else if ((tk = consume_keyword(">=")))
            node = binop(ND_LTE, shift(), node, tk->loc);
        else break;
    }
    return node;
}

Node *shift() {
    Node *node = add();
    for (Token *tk;;) {
        if ((tk = consume_keyword("<<")))
            node = binop(ND_LSH, node, add(), tk->loc);
        else if ((tk = consume_keyword(">>")))
            node = binop(ND_RSH, node, add(), tk->loc);
        else break;
    }
    return node;
}

Node *add() {
    Node *node = mul();
    for (Token *tk;;) {
        if ((tk = consume_keyword("+")))
            node = binop(ND_ADD, node, mul(), tk->loc);
        else if ((tk = consume_keyword("-")))
            node = binop(ND_SUB, node, mul(), tk->loc);
        else break;
    }
    return node;
}

Node *mul() {
    Node *node = unary();
    for (Token *tk;;) {
        if ((tk = consume_keyword("*")))
            node = binop(ND_MUL, node, unary(), tk->loc);
        else if ((tk = consume_keyword("/")))
            node = binop(ND_DIV, node, unary(), tk->loc);
        else if ((tk = consume_keyword("%")))
            node = binop(ND_MOD, node, unary(), tk->loc);
        else break;
    }
    return node;
}

Node *unary() {
    Token *tk;
    return (tk = consume_keyword("sizeof")) ? binop(ND_SIZEOF, unary(), NULL, tk->loc)
         : (tk = consume_keyword("++"))     ? binop(ND_PREINCR, unary(), NULL, tk->loc)
         : (tk = consume_keyword("--"))     ? binop(ND_PREDECR, unary(), NULL, tk->loc)
         : consume_keyword("+")             ? unary()
         : (tk = consume_keyword("-"))      ? binop(ND_SUB, mknum(0, tk->loc), unary(), tk->loc)
         : (tk = consume_keyword("&"))      ? binop(ND_ADDR, mul(), NULL, tk->loc)
         : (tk = consume_keyword("*"))      ? binop(ND_DEREF, mul(), NULL, tk->loc)
         : (tk = consume_keyword("!"))      ? binop(ND_NEG, unary(), NULL, tk->loc)
         : (tk = consume_keyword("~"))      ? binop(ND_BCOMPL, unary(), NULL, tk->loc)
         : postfix();
}

Node *postfix() {
    Node *node = primary();
    for (Token *tk;;) {
        if ((tk = consume_keyword("["))) {
            Node *index_node = expr();
            expect_keyword("]");

            Node *add_node = binop(ND_ADD, node, index_node, tk->loc);
            Node *next_node = binop(ND_DEREF, add_node, NULL, tk->loc);
            node = next_node;
        } else if ((tk = consume_keyword("."))) {
            Token *attr = expect(TK_IDT);
            Node *attr_node = binop(ND_ATTR, node, NULL, tk->loc);
            attr_node->attr = attr;
            node = attr_node;
        } else if ((tk = consume_keyword("->"))) {
            Token *attr = expect(TK_IDT);
            Node *l = binop(ND_DEREF, node, NULL, tk->loc);
            Node *next_node = binop(ND_ATTR, l, NULL, tk->loc);
            next_node->attr = attr;
            node = next_node;
        } else if ((tk = consume_keyword("++")))
            node = binop(ND_POSTINCR, node, NULL, tk->loc);
        else if ((tk = consume_keyword("--")))
            node = binop(ND_POSTDECR, node, NULL, tk->loc);
        else
            break;
    }

    return node;
}

Node *primary() {
    Token *tk = NULL;
    Node *node = NULL;
    if ((tk = consume_keyword("("))) {
        node = expr();
        expect_keyword(")");
    }

    if ((tk = consume(TK_IDT))) {
        if (consume_keyword("(")) { // CALL
            node = mknode(ND_CALL, tk->loc);
            node->name = tk->str;
            node->block = args();
        } else { // variable
            node = mknode(ND_VAR, tk->loc);
            node->name = tk->str;
        }
    } else if ((tk = consume(TK_NUM))) { // number
        node = mknum(tk->val, tk->loc);
    } else if ((tk = consume(TK_STRING))) {
        node = mknode(ND_STRING, tk->loc);
        node->type = type_ptr_char;

        StringBuilder *sb = strbld_new();
        strbld_append_str(sb, tk->str);
        while ((tk = consume(TK_STRING)))
            strbld_append_str(sb, tk->str);
        node->name = strbld_build(sb);
    } else if ((tk = consume(TK_CHAR))) {
        node = mknode(ND_CHAR, tk->loc);
        node->val = tk->val;
        node->type = type_char;
    }

    if (node == NULL) {
        tk = lookahead_any();
        error_loc(tk->loc, "invalid expression or statement");
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
    Type *typ = env_find(aliases, tk->str);
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
        || ((tk = lookahead(TK_IDT)) != NULL
                && env_find(aliases, tk->str) != NULL);
}

Type *consume_type_identifier() {
    Token *id = lookahead(TK_IDT);
    Type *typ = env_find(aliases, id->str);
    if (typ != NULL)
        index++;
    return typ;
}

Type *consume_type_pre() {
    Token *tk;
    Type *typ;
    if ((tk = consume_keyword("struct"))) {
        Token *strc_id = expect(TK_IDT);
        typ = env_find(structs, strc_id->str);
    } else
        typ = consume_type_identifier();

    if (typ == NULL)
        return NULL;

    typ = type_pointer(typ);
    return typ;
}

// Node helpers

Node *mknode(Node_kind kind, Location* loc) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->loc = loc;
    return node;
}

Node *binop(Node_kind kind, Node *lhs, Node *rhs, Location *loc) {
    Node *node = mknode(kind, loc);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *mknum(int v, Location *loc) {
    Node *node = mknode(ND_NUM, loc);
    node->type = type_int;
    node->kind = ND_NUM;
    node->val = v;
    return node;
}
