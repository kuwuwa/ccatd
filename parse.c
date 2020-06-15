#include "ccatd.h"

int index = 0;
Vec *functions;
Map *global_vars;
Environment *variable_env;
Environment *struct_env;
Environment *enum_env;
Environment *builtin_aliases;
Environment *aliases;

void toplevel();
Type *consume_type_spec();
Type *type_spec();
Node *consume_declarator(Type*);
Node *declarator(Type*);

Func *parse_func(Node *decl);
Type *parse_struct(Location *start);
Type *parse_enum(Location *start);

Vec *block();
Node *stmt();

Node *initializer();

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
Token *expect(Token_kind kind);
Token *expect_keyword(char *str);
Token *consume_value_identifier();
Token *value_identifier();
Type *consume_type_identifier();

Node *mknode(Node_kind kind, Location *loc);
Node *binop(Node_kind kind, Node *lhs, Node *rhs, Location *loc);
Node *mknum(int v, Location *loc);

// Parse

void parse() {
    variable_env = env_new(NULL);
    variable_env->map = global_vars;

    struct_env = env_new(NULL);
    enum_env = env_new(NULL);
    aliases = env_new(builtin_aliases);

    int len = vec_len(tokens);
    while (index < len)
        toplevel();
}

void toplevel() {
    Token *tk;
    if ((tk = consume_keyword("typedef"))) {
        Type *typ = type_spec();
        Node *decl = declarator(typ);
        expect_keyword(";");

        // TODO: Ideally want to stop this ad-hoc
        Type *aliased = calloc(1, sizeof(Type));
        *aliased = *typ;
        aliased->enum_decl = false;

        env_push(aliases, decl->name, aliased);
        return;
    }

    bool is_extern = consume_keyword("extern");

    Type *typ = type_spec();
    Node *decl = consume_declarator(typ);
    if (decl != NULL && is_func(decl->type)) {
        Func *func = parse_func(decl);
        if (consume_keyword(";")) {
            func->is_extern = true;
        } else {
            if (is_extern)
                error_loc(decl->loc, "[parse] prototype function declaration shouldn't have a body");
            func->is_extern = false;
            func->block = block();
            func->global_vars = map_new();
            for (int i = 0; i < vec_len(global_vars->values); i++) {
                Node *gvar = vec_at(global_vars->values, i);
                map_put(func->global_vars, gvar->name, gvar);
            }
        }
        vec_push(functions, func);
    } else {
        if (decl != NULL) {
            decl->kind = ND_GVAR;
            decl->is_extern = is_extern;

            decl->rhs = NULL;
            if (consume_keyword("=")) {
                if (is_extern)
                    error_loc(tk->loc, "[parse] extern variable declaration shouldn't have a value");
                decl->rhs = initializer();
            }

            map_put(global_vars, decl->name, decl);
        }
        expect_keyword(";");

    }
}

Node *param() {
    Type *typ = type_spec();
    Node *p = declarator(typ);
    p->kind = ND_VAR;
    return p;
}

void params(Func *fundecl) {
    fundecl->params = vec_new();
    expect_keyword("(");
    if (consume_keyword(")"))
        return;
    if (consume_keyword("...")) {
        fundecl->is_varargs = true;
        expect_keyword(")");
        return;
    }
    vec_push(fundecl->params, param());
    while (!consume_keyword(")")) {
        expect_keyword(",");
        if (consume_keyword("...")) {
            fundecl->is_varargs = true;
            expect_keyword(")");
            return;
        }
        vec_push(fundecl->params, param());
    }
}

Func *parse_func(Node *decl) {
    Func *func = calloc(1, sizeof(Func));
    func->loc = decl->loc;
    func->name = decl->name;
    func->ret_type = decl->type->ptr_to;
    params(func);
    return func;
}

Type *parse_struct(Location *start) {
    Token *strc_id = consume(TK_IDT);
    Vec *fields = NULL;
    if (consume_keyword("{")) {
        fields = vec_new();
        while (!consume_keyword("}")) {
            Type *typ = type_spec();
            Node *fld = declarator(typ);
            fld->kind = ND_VAR;

            vec_push(fields, fld);
            expect_keyword(";");
        }
    }

    if (strc_id == NULL && fields == NULL)
        error_loc(start, "[parse] invalid struct statement");

    Struct *strc = calloc(1, sizeof(Struct));
    strc->name = (strc_id == NULL) ? NULL : strc_id->str;
    strc->loc = start;

    Type *typ = calloc(1, sizeof(Type));
    typ->ty = TY_STRUCT;
    typ->strct = strc;

    Type *existing =
        (strc->name == NULL) ? NULL : env_find(struct_env, strc->name);
    if (existing == NULL) {
        strc->fields = fields;
        if (strc->name != NULL)
            env_push(struct_env, strc->name, typ);
    } else {
        Struct *existing_strc = existing->strct;
        if (existing_strc->fields != NULL && fields != NULL)
            error_loc(start, "[parse] duplicate struct declaration");

        strc->fields = (fields == NULL)
            ? (fields = existing_strc->fields)
            : (existing_strc->fields = fields);
    }

    return typ;
}

Type *parse_enum(Location *start) {
    Token *enum_id = consume(TK_IDT);

    Type *typ = calloc(1, sizeof(Type));
    typ->ty = TY_ENUM;

    if (consume_keyword("{")) {
        typ->enums = vec_new();
        vec_push(typ->enums, expect(TK_IDT));
        while (!consume_keyword("}")) {
            expect_keyword(",");
            vec_push(typ->enums, expect(TK_IDT));
        }
    }

    bool enum_decl = typ->enum_decl = typ->enums != NULL;
    if (typ->enum_decl) {
        int len = vec_len(typ->enums);
        for (int i = 0; i < len; i++) {
            Token *tk = vec_at(typ->enums, i);
            Node *id = mknode(ND_GVAR, tk->loc);
            id->type = typ;
            id->name = tk->str;
            map_put(variable_env->map, id->name, id);
        }
    }

    if (enum_id == NULL && typ->enums == NULL)
        error_loc(start, "[parse] invalid struct statement");

    if (enum_id != NULL) {
        Vec *existing = map_find(enum_env->map, enum_id->str);
        if (enum_decl && existing != NULL)
            error_loc(start, "[parse] duplicate enum type");

        if (enum_decl)
            env_push(enum_env, enum_id->str, typ->enums);
        else {
            existing = env_find(enum_env, enum_id->str);
            if (existing == NULL)
                error_loc(start, "[parse] undefined enum type");
            typ->enums = existing;
        }
    }

    return typ;
}

Type *consume_type_spec() {
    Token *tk;
    if ((tk = consume_keyword("struct")))
        return parse_struct(tk->loc);
    if ((tk = consume_keyword("enum")))
        return parse_enum(tk->loc);
    return consume_type_identifier();
}

Type *type_spec() {
    Type *typ = consume_type_spec();
    if (typ == NULL)
        error_loc(lookahead_any()->loc, "[parse] type expected");
    return typ;
}

Node *consume_declarator(Type *spec) {
    Type *typ = spec;
    while (consume_keyword("*")) typ = ptr_of(typ);

    Token *id = consume_value_identifier();
    if (id == NULL) {
        if (typ != spec)
            error_loc(lookahead_any()->loc, "[parse] identifier expected in declarator");
        return NULL;
    }

    Node *decl = calloc(1, sizeof(Node));
    decl->loc = id->loc;
    decl->name = id->str;

    if (lookahead_keyword("(")) {
        decl->type = func_returns(typ);
        return decl;
    }

    decl->type = typ;
    while (consume_keyword("[")) {
        Token *len = consume(TK_NUM);
        decl->type = array_of(decl->type, len->val);
        expect_keyword("]");
    }
    return decl;
}

Node *declarator(Type *spec) {
    Node *decl = consume_declarator(spec);
    if (decl == NULL)
        error_loc(lookahead_any()->loc, "[parse] identifier expected in declarator");
    return decl;
}

Vec *block() {
    variable_env = env_new(variable_env);
    struct_env = env_new(struct_env);
    enum_env = env_new(enum_env);

    Vec *vec = vec_new();
    expect_keyword("{");
    while (!consume_keyword("}"))
        vec_push(vec, stmt());

    variable_env = env_next(variable_env);
    struct_env = env_next(struct_env);
    enum_env = env_next(enum_env);
    return vec;
}

Node *stmt() {
    Token *tk;
    Node *node;
    Type *typ;
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
            if ((typ = consume_type_spec())) {
                Node *var = declarator(typ);
                var->kind = ND_VAR;
                Node *e = consume_keyword("=") ? initializer() : NULL;
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
    } else if ((typ = consume_type_spec())) {
        Node *lhs = declarator(typ);
        lhs->kind = ND_VAR;
        Node *rhs = consume_keyword("=") ? initializer() : NULL;
        node = binop(ND_VARDECL, lhs, rhs, lhs->loc);
        expect_keyword(";");
    } else {
        node = expr();
        expect_keyword(";");
    }
    return node;
}

// Expressions

Node *initializer_list(Location *start) {
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

Node *initializer() {
    Token *tk = NULL;
    if ((tk = consume_keyword("{")))
        return initializer_list(tk->loc);
    return expr();
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

Node *parse_sizeof(Location *loc) {
    if (consume_keyword("(")) {
        Node *node = parse_sizeof(loc);
        expect_keyword(")");
        return node;
    }
    Type *typ = consume_type_spec();
    if (typ != NULL) {
        while (consume_keyword("*")) typ = ptr_of(typ);
        Node *node = calloc(1, sizeof(Node));
        node->type = typ;
        return binop(ND_SIZEOF, node, NULL, loc);
    }
    return binop(ND_SIZEOF, unary(), NULL, loc);
}

Node *unary() {
    Token *tk;
    return (tk = consume_keyword("sizeof")) ? parse_sizeof(tk->loc)
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
            Token *attr = value_identifier();
            Node *attr_node = binop(ND_ATTR, node, NULL, tk->loc);
            attr_node->attr = attr;
            node = attr_node;
        } else if ((tk = consume_keyword("->"))) {
            Token *attr = value_identifier();
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

    if ((tk = consume_value_identifier())) {
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

Token *consume_value_identifier() {
    Token *tk = lookahead(TK_IDT);
    if (tk == NULL)
        return NULL;
    Type *typ = env_find(aliases, tk->str);
    if (typ != NULL)
        return NULL;
    index++;
    return tk;
}

Token *value_identifier() {
    Token *tk = lookahead(TK_IDT);
    Type *typ = env_find(aliases, tk->str);
    if (typ != NULL)
        error_loc(tk->loc, "[parse] type");
    index++;
    return tk;
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

Type *consume_type_identifier() {
    Token *id = lookahead(TK_IDT);
    if (id == NULL)
        return NULL;
    Type *typ = env_find(aliases, id->str);
    if (typ != NULL)
        index++;
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
