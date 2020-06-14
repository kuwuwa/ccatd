#include <string.h>

#include "ccatd.h"

Map *func_env;
Map *global_env;
Environment *local_vars;
int jump_id = 0;
Vec *break_labels;
Vec *continue_labels;
int scoped_stack_space;
int max_scoped_stack_space;

// Global

void sema_globals();
void sema_const(Node*);
void sema_const_aux(Node*);
void sema_const_array(Type*, Node*);

// Functions, statements and expressions

void sema_func(Func *func);
void sema_block(Vec*, Func*);
void sema_stmt(Node*, Func*);
void sema_expr(Node*, Func*);
void sema_lval(Node*, Func*);
void sema_array(Type*, Node*, Func*);
void sema_type(Type*);

// Helpers

bool assignable(Type *lhs, Type *rhs);
bool eq_type(Type *lhs, Type *rhs);
char *gen_loop_label(char *prefix);

// Global

void sema_globals() {
    int globals_len = vec_len(global_vars->values);
    for (int i = 0; i < globals_len; i++) {
        Node *g = vec_at(global_vars->values, i);
        for (int j = 0; j < i; j++) {
            Node *h = vec_at(global_vars->values, j);
            if (!strcmp(g->name, h->name)) {
                if (!g->is_extern && !h->is_extern)
                    error_loc(g->loc, "[semantic] duplicate global variable found");
                if (!eq_type(g->type, h->type))
                    error_loc(g->loc, "[semantic] type mismatch between variable declarations");
            }
        }
    }

    global_env = map_new();
    for (int i = 0; i < globals_len; i++) {
        Node *g = vec_at(global_vars->values, i);
        if (g->is_extern)
            continue;
        if (g->kind != ND_GVAR)
            error_loc(g->loc, "[semantic] a global variable expected");

        if (g->rhs != NULL) {
            sema_const(g);
            if (!assignable(g->type, g->rhs->type))
                error_loc(g->loc, "[semantic] type mismatch in a global variable initialization");
        }
        map_put(global_env, g->name, g);
    }
}

void sema_const(Node *global) {
    if (global->rhs->kind == ND_ARRAY) {
        sema_const_array(global->type, global->rhs);
        return;
    }
    sema_const_aux(global->rhs);
}

void sema_const_aux(Node *node) {
    if (node->kind == ND_NUM) {
        if (!eq_type(type_int, node->type))
            error("[semantic] type mismatch in a global variable definition");
        return;
    }
    if (node->kind == ND_STRING) {
        if (!eq_type(type_ptr_char, node->type))
            error("[semantic] type mismatch in a global variable definition");
        return;
    }

    if (node->kind == ND_ADDR) {
        Node *e = node->lhs;
        if (e->kind != ND_VAR)
            error_loc(e->loc, "[semantic] a left value expected");
        sema_const_aux(e);

        node->type = ptr_of(e->type);
        return;
    }

    if (node->kind == ND_VAR) {
        Node *resolved = map_find(global_env, node->name);
        if (resolved == NULL)
            error_loc(node->loc, "[semantic] undefined variable");
        *node = *resolved;
        return;
    }

    if (node->kind == ND_ADD) {
        sema_const_aux(node->lhs);
        sema_const_aux(node->rhs);
        Type *lty = node->lhs->type;
        Type *rty = node->rhs->type;

        if (is_integer(lty) && is_integer(rty))
            node->type = binary_int_op_result(lty, rty);
        else if ((is_pointer_compat(lty) && is_integer(rty)) ||
                (is_integer(lty) && is_pointer_compat(rty)))
            node->type = is_pointer_compat(lty) ? lty : rty;
        else
            error_loc(node->loc, "[semantic] unsupported addition in a global variable initialization");
        return;
    }

    if (node->kind == ND_SUB) {
        sema_const_aux(node->lhs);
        sema_const_aux(node->rhs);
        Type *lty = node->lhs->type;
        Type *rty = node->rhs->type;

        if (is_integer(lty) && is_integer(rty))
            node->type = binary_int_op_result(lty, rty);
        else if (is_pointer_compat(lty) && is_integer(rty))
            node->type = lty;
        else
            error_loc(node->loc, "[semantic] unsupported addition in a global variable initialization");
        return;
    }

    error_loc(node->loc, "[semantic] unsupported expression in a global variable initialization");
}

void sema_const_array(Type *type, Node *node) {
    int array_len = vec_len(node->block);
    if (array_len > type->array_size)
        error("[semantic] too long array");
    for (int i = 0; i < array_len; i++) {
        Node *e = vec_at(node->block, i);
        if (e->kind == ND_ARRAY)
            sema_const_array(type->ptr_to, e);
        else {
            sema_const_aux(e);
            if (!assignable(type->ptr_to, e->type))
                error_loc(e->loc, "[semantic] type mismatch");
        }
    }
    node->type = type;
}

// Functions, statements and expressions

void sema_type(Type* typ) {
    if (typ->ty == TY_ENUM) {
        if (typ->enum_decl) {
            int len = vec_len(typ->enums);
            for (int i = 0; i < len; i++) {
                Token *e = vec_at(typ->enums, i);
                if (map_find(local_vars->map, e->str) != NULL)
                    error_loc(e->loc, "[semantic] duplicate identifier");

                Node *lvar = calloc(1, sizeof(Node));
                lvar->kind = ND_VAR;
                lvar->type = typ;
                lvar->name = e->str;
                env_push(local_vars, e->str, lvar);
            }
        }
    }
    if (typ->ty == TY_PTR)
        sema_type(typ->ptr_to);
}

void sema_func(Func *func) {
    int params_len = vec_len(func->params);
    // no duplicate parameter
    for (int i = 0; i < params_len; i++) {
        Node *pi = vec_at(func->params, i);
        if (pi->type == type_void)
            error_loc(pi->loc, "[semantic] a parameter of void type is not allowed");

        for (int j = i+1; j < params_len; j++) {
            Node *pj = vec_at(func->params, j);
            if (!strcmp(pi->name, pj->name))
                error_loc(pi->loc, "%s: duplicate parameter `%s'", func->name, pi->name);
        }
    }

    if (func->is_extern) {
        map_put(func_env, func->name, func);
        return;
    }

    Func *g = map_find(func_env, func->name);
    if (g != NULL) {
        if (!g->is_extern)
            error_loc(func->loc, "[semantic] a name conflict between functions");

        int params_len = vec_len(func->params);
        bool matched = params_len == vec_len(g->params);
        for (int i = 0; matched && i < params_len; i++) {
            Node *fi = vec_at(func->params, i);
            Node *gi = vec_at(g->params, i);
            matched = matched && eq_type(fi->type, gi->type);
        }

        if (!matched)
            error_loc(func->loc, "[semantic] function signature doesn't match with a previous declaration");
    }

    map_put(func_env, func->name, func);

    // block
    local_vars = env_new(NULL);
    break_labels = vec_new();
    continue_labels = vec_new();
    for (int i = 0; i < params_len; i++) {
        Node* param = vec_at(func->params, i);
        param->val = 8 * (i + 1);
        env_push(local_vars, param->name, param);
    }
    max_scoped_stack_space = scoped_stack_space = 8 * params_len;

    sema_block(func->block, func);
    func->offset = max_scoped_stack_space;
}

void sema_block(Vec *block, Func *func) {
    local_vars = env_new(local_vars);
    int revert_scoped_stack_space = scoped_stack_space;

    int block_len = vec_len(block);
    for (int i = 0; i < block_len; i++)
        sema_stmt(vec_at(block, i), func);

    local_vars = env_next(local_vars);
    scoped_stack_space = revert_scoped_stack_space;
}

void sema_case(Node *node) {
    if (node->kind == ND_NUM)
        return;
    error_loc(node->loc, "[semantic] a term cannot be deduced to an integer");
}

void sema_switch(Node *node, Func *func) {
    char *label = node->name = gen_loop_label("switch");
    int len = vec_len(node->block);

    vec_push(break_labels, label);
    sema_expr(node->cond, func);
    for (int i = 0; i < len; i++) {
        Node *stmt = vec_at(node->block, i);
        if (stmt->kind == ND_CASE) {
            sema_case(stmt->lhs);
            stmt->name = gen_loop_label("case");
        } else if (stmt->kind == ND_DEFAULT) {
            stmt->name = gen_loop_label("default");
        } else
            sema_stmt(stmt, func);
    }
    vec_pop(break_labels);
}

void sema_stmt(Node *node, Func *func) {
    if (node->kind == ND_VARDECL) {
        Node *lvar = map_find(local_vars->map, node->lhs->name);
        if (lvar != NULL)
            error_loc(node->loc, "[semantic] duplicate variable declaration");

        if (node->lhs->type == type_void)
            error_loc(node->loc, "[semantic] declaring a variable of void type is not fllowed");
        sema_type(node->lhs->type);

        if (node->rhs != NULL) {
            if (node->lhs->type->ty == TY_ARRAY) {
                if (node->lhs->type->ptr_to->ty == TY_CHAR && node->rhs->kind == ND_STRING)
                    ;
                else if (node->rhs->kind == ND_ARRAY)
                    sema_array(node->lhs->type, node->rhs, func);
                else
                    error_loc(node->loc, "[semantic] unsupported array initialization");
            } else {
                sema_expr(node->rhs, func);
                if (!assignable(node->lhs->type, node->rhs->type))
                    error_loc(node->loc, "[semantic] type mismatch in a variable declaration");
            }
        }

        scoped_stack_space += type_size(node->lhs->type);
        node->lhs->val = scoped_stack_space;
        env_push(local_vars, node->lhs->name, node->lhs);
        max_scoped_stack_space = scoped_stack_space > max_scoped_stack_space
            ? scoped_stack_space
            : max_scoped_stack_space;

        node->type = node->lhs->type;
        return;
    }
    if (node->kind == ND_RETURN) {
        Type *typ = node->lhs == NULL
            ? type_void
            : (sema_expr(node->lhs, func), node->lhs->type);
        if (!eq_type(typ, func->ret_type))
            error_loc(node->loc, "[semantic] type mismatch in a return statement");
        return;
    }
    if (node->kind == ND_IF) {
        sema_expr(node->cond, func);
        sema_stmt(node->lhs, func);
        if (node->rhs != NULL)
            sema_stmt(node->rhs, func);
        return;
    }
    if (node->kind == ND_WHILE) {
        char *loop_label = node->name = gen_loop_label("while");
        sema_expr(node->cond, func);
        vec_push(break_labels, loop_label);
        vec_push(continue_labels, loop_label);
        sema_stmt(node->body, func);
        vec_pop(break_labels);
        vec_pop(continue_labels);
        return;
    }
    if (node->kind == ND_FOR) {
        char *loop_label = node->name = gen_loop_label("for");
        vec_push(break_labels, loop_label);
        vec_push(continue_labels, loop_label);
        Vec *for_block = vec_new();
        if (node->lhs != NULL)
            vec_push(for_block, node->lhs);
        if (node->cond != NULL)
            vec_push(for_block, node->cond);
        if (node->rhs != NULL)
            vec_push(for_block, node->rhs);
        vec_push(for_block, node->body);
        sema_block(for_block, func);
        vec_pop(break_labels);
        vec_pop(continue_labels);
        return;
    }
    if (node->kind == ND_DOWHILE) {
        char *loop_label = node->name = gen_loop_label("dowhile");
        vec_push(break_labels, loop_label);
        vec_push(continue_labels, loop_label);
        sema_stmt(node->body, func);
        vec_pop(break_labels);
        vec_pop(continue_labels);
        sema_expr(node->cond, func);
        return;
    }
    if (node->kind == ND_BREAK || node->kind == ND_CONTINUE) {
        Vec *labels = node->kind == ND_BREAK ? break_labels : continue_labels;
        if (vec_len(labels) == 0)
            error_loc(node->loc, "break/continue should be inside a loop statement");
        char *label = vec_at(labels, vec_len(labels)-1);
        node->name = label;
        return;
    }
    if (node->kind == ND_SWITCH) {
        sema_switch(node, func);
        return;
    }
    if (node->kind == ND_CASE)
        error_loc(node->loc, "`case' should be under a switch statement");
    if (node->kind == ND_DEFAULT)
        error_loc(node->loc, "`default' should be under a switch statement");
    if (node->kind == ND_BLOCK) {
        sema_block(node->block, func);
        return;
    }

    sema_expr(node, func);
}

Type *sema_expr_arith(Node *node, Func *func) {
    if (!(ND_ADD <= node->kind && node->kind <= ND_LOR))
        return NULL;

    sema_expr(node->lhs, func);
    sema_expr(node->rhs, func);
    Type* lty = node->lhs->type;
    Type* rty = node->rhs->type;
    Location *loc = node->loc;

    Type *ret;
    switch (node->kind) {
    case ND_ADD:
        if ((ret = binary_int_op_result(lty, rty)) != NULL)
            return ret;
        if (is_pointer_compat(lty) && is_integer(rty))
            return coerce_pointer(lty);
        if (is_integer(lty) && is_pointer_compat(rty))
            return coerce_pointer(rty);
        error_loc(loc, "[semantic] unsupported addition");
    case ND_SUB:
        if ((ret = binary_int_op_result(lty, rty)) != NULL)
            return ret;
        if (is_pointer_compat(lty) && is_pointer_compat(rty)
                && eq_type(lty->ptr_to, rty->ptr_to))
            return type_int;
        if (is_pointer_compat(lty) && is_int(rty))
            return coerce_pointer(lty);
        error_loc(loc, "[semantic] unsupported subtraction");
    case ND_MUL:
        if ((ret = binary_int_op_result(lty, rty)) != NULL)
            return ret;
        error_loc(loc, "[semantic] unsupported multiplication");
    case ND_DIV:
        if ((ret = binary_int_op_result(lty, rty)) != NULL)
            return ret;
        error_loc(loc, "[semantic] unsupported division");
    case ND_MOD:
        if ((ret = binary_int_op_result(lty, rty)) != NULL)
            return ret;
        error_loc(loc, "[semantic] unsupported modulo");
    case ND_EQ: case ND_NEQ:
    case ND_LT: case ND_LTE:
    case ND_LAND: case ND_LOR: // TODO: Probably some check is needed
        return type_int;
    case ND_IOR: case ND_XOR: case ND_AND:
        if ((ret = binary_int_op_result(lty, rty)) != NULL)
            return ret;
        error_loc(loc, "[semantic] type mismatch in an AND/OR expression");
    case ND_LSH: case ND_RSH:
        if (binary_int_op_result(lty, rty) == NULL)
            error_loc(loc, "[semantic] type mismatch in a shift expression");
        return lty;
    default:
        return NULL;
    }
}

Type *sema_expr_assign(Node *node, Func *func) {
    if (!(ND_ADDEQ <= node->kind && node->kind <= ND_XOREQ))
        return NULL;

    sema_lval(node->lhs, func);
    sema_expr(node->rhs, func);
    Type* lty = node->lhs->type;
    Type* rty = node->rhs->type;
    Location *loc = node->loc;

    switch (node->kind) {
    case ND_ADDEQ:
        if (is_pointer(lty) && is_integer(rty))
            return lty;
        break;
    case ND_SUBEQ:
        if (is_pointer(lty) && is_integer(rty))
            return lty;
        break;
    default:
        ;
    }

    if (binary_int_op_result(lty, rty) == NULL)
        error_loc(loc, "[semantic] unsupported assignment");

    return lty;
}

void sema_expr(Node* node, Func *func) {
    switch (node->kind) {
    case ND_NUM: case ND_STRING: case ND_CHAR:
        return;
    case ND_VAR: {
        Node *resolved_local = env_find(local_vars, node->name);
        if (resolved_local != NULL) {
            *node = *resolved_local;
            return;
        }

        Node *resolved_global = map_find(func->global_vars, node->name);
        if (resolved_global == NULL)
            error_loc(node->loc, "[semantic] undefined variable");
        *node = *resolved_global;
        return;
    }
    case ND_SEQ:
        sema_expr(node->lhs, func);
        sema_expr(node->rhs, func);
        node->type = node->rhs->type;
        return;
    case ND_ASGN:
        sema_lval(node->lhs, func);
        sema_expr(node->rhs, func);
        if (!assignable(node->lhs->type, node->rhs->type))
            error_loc(node->loc, "[semantic] type mismatch in an assignment statment");
        node->type = node->lhs->type;
        return;
    case ND_ADDR:
        sema_lval(node->lhs, func);
        node->type = ptr_of(node->lhs->type);
        return;
    case ND_DEREF:
        sema_expr(node->lhs, func);
        if (!is_pointer_compat(node->lhs->type))
            error_loc(node->loc, "dereferencing non-pointer is not allowed");
        node->type = node->lhs->type->ptr_to;
        return;
    case ND_SIZEOF:
        sema_expr(node->lhs, func);
        node->type = type_int;
        node->val = type_size(node->lhs->type);
        return;
    case ND_NEG:
        sema_expr(node->lhs, func);
        node->type = node->lhs->type;
        return;
    case ND_BCOMPL:
        sema_expr(node->lhs, func);
        node->type = node->lhs->type;
        return;
    case ND_CALL: {
        Func *f = map_find(func_env, node->name);
        if (f == NULL)
            error_loc(node->loc, "undefined function");

        int params_len = vec_len(node->block);
        if (params_len != vec_len(f->params))
            error_loc(node->loc, "invalid number of argument(s)");
        for (int i = 0; i < params_len; i++)
            sema_expr(vec_at(node->block, i), func);

        node->type = f->ret_type;
        return;
    }
    case ND_COND:
        sema_expr(node->cond, func);
        sema_expr(node->lhs, func);
        sema_expr(node->rhs, func);

        if (!eq_type(node->lhs->type, node->rhs->type))
            error_loc(node->loc, "[semantic] type mismatch in a conditional expression");
        return;
    case ND_ATTR: {
        sema_expr(node->lhs, func);
        if (node->lhs->type->ty != TY_STRUCT)
            error_loc(node->loc, "[semantic] attribute access to a non-struct value");

        Struct *strc = node->lhs->type->strct;
        int len = vec_len(strc->fields);
        int offset = 0;
        for (int i = 0; i < len; i++) {
            Node *field = vec_at(strc->fields, i);
            if (!strcmp(field->name, node->attr->str)) {
                node->type = field->type;
                node->val = offset;
                return;
            }
            offset += type_size(field->type);
        }
        error_loc(node->loc, "[semantic] the given attribute doesn't exist");
    }
    case ND_PREINCR: case ND_PREDECR: case ND_POSTINCR: case ND_POSTDECR: {
        sema_lval(node->lhs, func);
        Type *lty = node->lhs->type;
        node->type = lty;
        if (is_integer(lty) || lty->ty == TY_PTR)
            return;
        error_loc(node->loc, "[semantic] unexpected type of value in increment/decrement expression");
    }
    default:
        break;
    }

    node->type = sema_expr_arith(node, func);
    if (node->type != NULL)
        return;

    node->type = sema_expr_assign(node, func);
    if (node->type != NULL)
        return;

    error_loc(node->loc, "unsupported feature");
}

void sema_array(Type* ty, Node* arr, Func *func) {
    int array_len = vec_len(arr->block);
    if (array_len > ty->array_size)
        error_loc(arr->loc, "[semantic] too long array");

    Type *elem_type = ty->ptr_to;
    for (int i = 0; i < array_len; i++) {
        Node *e = vec_at(arr->block, i);
        sema_expr(e, func);
        if (!eq_type(elem_type, e->type))
            error_loc(e->loc, "[semantic] type mismatch in array");
    }

    arr->type = ty;
}

void sema_lval(Node *node, Func *func) {
    Node_kind k = node->kind;
    if (k != ND_VAR && k != ND_GVAR && k != ND_DEREF && k != ND_ATTR)
        error_loc(node->loc, "[semantic] should be lvalue");
    sema_expr(node, func);
}

// Helpers

// TODO: Needs to be improved
bool assignable(Type *lhs, Type *rhs) {
    if (lhs == type_void || rhs == type_void)
        return false;
    if (is_integer(lhs) && is_integer(rhs))
        return true;
    if (is_pointer_compat(lhs))
        return is_pointer_compat(rhs);

    debug("[internal] assignable %d %d", lhs->ty, rhs->ty);
    return false;
}

char *gen_loop_label(char *prefix) {
    int len = strlen(prefix) + 11;
    char *str = calloc(len, sizeof(char));
    sprintf(str, "%s%d", prefix, jump_id++);
    return str;
}
