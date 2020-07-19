#include "ccatd.h"

int index = 0;
Vec *functions;
Map *global_vars;
Environment *variable_env;
Environment *struct_env;
Environment *enum_env;
Environment *builtin_aliases;
Environment *aliases;

static void toplevel();
static Type *consume_type_spec();
static Type *type_spec();
static Node *consume_declarator(Type* typ);
static Node *declarator(Type* typ);

static Func *parse_func(Node *decl);
static Type *parse_struct(Location *start);
static Type *parse_enum(Location *start);

static Vec *block();
static Node *stmt();

static Node *initializer();

static Node *expr();
static Node *assignment();
static Node *conditional();
static Node *logical_or();
static Node *logical_and();
static Node *inclusive_or();
static Node *exclusive_or();
static Node *and_expr();
static Node *equality();
static Node *relational();
static Node *shift();
static Node *add();
static Node *mul();
static Node *cast();
static Node *unary();
static Node *postfix();
static Node *primary();
static Vec *args();

static Token *lookahead_any();
static Token *lookahead(Token_kind kind);
static Token *lookahead_keyword(char *str);
static Token *consume(Token_kind kind);
static Token *consume_keyword(char *str);
static Token *expect(Token_kind kind);
static Token *expect_keyword(char *str);
static Token *consume_value_identifier();
static Token *value_identifier();
static Type *consume_type_identifier();

static Node *mknode(Node_kind kind, Location *loc);
static Node *binop(Node_kind kind, Node *lhs, Node *rhs, Location *loc);
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

static void toplevel() {
    Token *tk;
    if ((tk = consume_keyword("typedef"))) {
        Type *typ = type_spec();
        Node *decl = declarator(typ);
        typ = decl->type;
        expect_keyword(";");

        Type *aliased = calloc(1, sizeof(Type));
        aliased->ty = typ->ty;
        aliased->ptr_to = typ->ptr_to;
        aliased->array_size = typ->array_size;
        aliased->strct = typ->strct;
        aliased->enum_decl = false;
        aliased->enums = typ->enums;

        env_push(aliases, decl->name, aliased);
        return;
    }

    bool is_extern = false;
    bool is_static = false;
    while (true) {
        if ((tk = consume_keyword("extern")) != NULL) {
            if (is_extern)
                error_loc(tk->loc, "[parse] duplicate extern qualifier");
            is_extern = true;
        } else if ((tk = consume_keyword("static")) != NULL) {
            if (is_extern)
                error_loc(tk->loc, "[parse] duplicate static qualifier");
            is_static = true;
        } else
            break;
    }

    Type *typ = type_spec();
    Node *decl = consume_declarator(typ);
    if (decl != NULL && is_func(decl->type)) {
        Func *func = parse_func(decl);
        func->is_static = is_static;
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
            decl->is_static = is_static;

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

static Node *param() {
    Type *typ = type_spec();
    Node *p = declarator(typ);
    p->kind = ND_VAR;
    return p;
}

static void params(Func *fundecl) {
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

static Func *parse_func(Node *decl) {
    Func *func = calloc(1, sizeof(Func));
    func->loc = decl->loc;
    func->name = decl->name;
    func->ret_type = decl->type->ptr_to;
    params(func);
    return func;
}

static Type *parse_struct(Location *start) {
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
        Type *local_strct = map_find(struct_env->map, strc->name);
        if (local_strct != NULL && fields != NULL) {
            if (local_strct->strct->fields != NULL)
                error_loc(start, "[parse] duplicate struct declaration");
            else
                local_strct->strct->fields = fields;
        }

        strc->fields = (fields == NULL)
            ? existing->strct->fields
            : fields;

        Vec *as = aliases->map->values;
        for (int i = 0; i < vec_len(as); i++) {
            Type *tya = vec_at(as, i);
            if (tya->ty != TY_STRUCT)
                continue;
            if (tya->strct->name != NULL && !strcmp(tya->strct->name, strc->name)) {
                tya->strct = strc;
                // char *k = vec_at(aliases->map->keys, i);
                // map_put(aliases->map, k, typ);
                break;
            }
        }
    }

    return typ;
}

static Type *parse_enum(Location *start) {
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
            id->is_enum = true;
            id->val = i;
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

static Type *consume_type_spec() {
    Token *tk;
    if ((tk = consume_keyword("struct")))
        return parse_struct(tk->loc);
    if ((tk = consume_keyword("enum")))
        return parse_enum(tk->loc);
    return consume_type_identifier();
}

static Type *type_spec() {
    Type *typ = consume_type_spec();
    if (typ == NULL)
        error_loc(lookahead_any()->loc, "[parse] type expected");
    return typ;
}

static Node *consume_declarator(Type *spec) {
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

static Node *declarator(Type *spec) {
    Node *decl = consume_declarator(spec);
    if (decl == NULL)
        error_loc(lookahead_any()->loc, "[parse] identifier expected in declarator");
    return decl;
}

static Vec *block() {
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

static Node *stmt() {
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

static Node *initializer_list(Location *start) {
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

static Node *initializer() {
    Token *tk = NULL;
    if ((tk = consume_keyword("{")))
        return initializer_list(tk->loc);
    return expr();
}

static Node *expr() {
    Node *node = assignment();
    for (Token *tk; (tk = consume_keyword(","));)
        node = binop(ND_SEQ, node, assignment(), tk->loc);
    return node;
}

// TODO: assignment ::= conditional | unary unary-op assignment
static Node *assignment() {
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

static Node *conditional() {
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

static Node *logical_or() {
    Node *node = logical_and();
    for (Token *tk; (tk = consume_keyword("||"));)
        node = binop(ND_LOR, node, logical_and(), tk->loc);
    return node;
}

static Node *logical_and() {
    Node *node = inclusive_or();
    for (Token *tk; (tk = consume_keyword("&&"));)
        node = binop(ND_LAND, node, inclusive_or(), tk->loc);
    return node;
}

static Node *inclusive_or() {
    Node *node = exclusive_or();
    for (Token *tk; (tk = consume_keyword("|"));)
        node = binop(ND_IOR, node, exclusive_or(), tk->loc);
    return node;
}

static Node *exclusive_or() {
    Node *node = and_expr();
    for (Token *tk; (tk = consume_keyword("^"));)
        node = binop(ND_XOR, node, and_expr(), tk->loc);
    return node;
}

static Node *and_expr() {
    Node *node = equality();
    for (Token *tk; (tk = consume_keyword("&"));)
        node = binop(ND_AND, node, equality(), tk->loc);
    return node;
}

static Node *equality() {
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

static Node *relational() {
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

static Node *shift() {
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

static Node *add() {
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

static Node *mul() {
    Node *node = cast();
    for (Token *tk;;) {
        if ((tk = consume_keyword("*")))
            node = binop(ND_MUL, node, cast(), tk->loc);
        else if ((tk = consume_keyword("/")))
            node = binop(ND_DIV, node, cast(), tk->loc);
        else if ((tk = consume_keyword("%")))
            node = binop(ND_MOD, node, cast(), tk->loc);
        else break;
    }
    return node;
}

static Node *consume_cast() {
    int backtrack = index;
    Token *start = consume_keyword("(");
    if (start == NULL)
        return NULL;

    Type *typ = consume_type_identifier();
    if (typ == NULL) {
        index = backtrack;
        return NULL;
    }
    while (consume_keyword("*"))
        typ = ptr_of(typ);
    expect_keyword(")");

    Node *node = mknode(ND_CAST, start->loc);
    node->type = typ;
    node->lhs = cast();
    return node;
}

static Node *cast() {
    Node *cast_expr = consume_cast();
    if (cast_expr != NULL)
        return cast_expr;

    return unary();
}

static Node *parse_sizeof(Location *loc) {
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

static Node *unary() {
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

static Node *postfix() {
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

static Node *primary() {
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
        node->name = tk->str;
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

static Vec *args() {
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

static Token *lookahead_any() {
    if (index >= vec_len(tokens))
        return NULL;
    return vec_at(tokens, index);
}

static Token *lookahead(Token_kind kind) {
    Token *tk = lookahead_any();
    return (tk != NULL && tk->kind == kind) ? tk : NULL;
}

static Token *lookahead_keyword(char *str) {
    Token *tk = lookahead(TK_KWD);
    return (tk != NULL && !strcmp(str, tk->str)) ? tk : NULL;
}

static Token *consume(Token_kind kind) {
    Token *tk = vec_at(tokens, index);
    if (tk->kind != kind)
        return NULL;
    index++;
    return tk;
}

static Token *consume_keyword(char *str) {
    Token *tk = lookahead(TK_KWD);
    if (tk == NULL || strcmp(str, tk->str))
        return NULL;
    index++;
    return tk;
}

static Token *consume_value_identifier() {
    Token *tk = lookahead(TK_IDT);
    if (tk == NULL)
        return NULL;
    Type *typ = env_find(aliases, tk->str);
    if (typ != NULL)
        return NULL;
    index++;
    return tk;
}

static Token *value_identifier() {
    Token *tk = lookahead(TK_IDT);
    Type *typ = env_find(aliases, tk->str);
    if (typ != NULL)
        error_loc(tk->loc, "[parse] type");
    index++;
    return tk;
}

static Token *expect(Token_kind kind) {
    Token *tk = lookahead_any();
    if (tk->kind != kind)
        error_loc(tk->loc, "%s expected\n", kind);
    index++;
    return tk;
}

static Token *expect_keyword(char* str) {
    Token *tk = lookahead_any();
    if (tk->kind != TK_KWD || strcmp(str, tk->str))
        error_loc(tk->loc, "\"%s\" expected", str);
    index++;
    return tk;
}

static Type *consume_type_identifier() {
    Token *id = lookahead(TK_IDT);
    if (id == NULL)
        return NULL;
    Type *typ = env_find(aliases, id->str);
    if (typ != NULL)
        index++;
    return typ;
}

// Node helpers

static Node *mknode(Node_kind kind, Location* loc) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->loc = loc;
    return node;
}

static Node *binop(Node_kind kind, Node *lhs, Node *rhs, Location *loc) {
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
