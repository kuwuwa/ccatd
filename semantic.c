#include <string.h>

#include "ccatd.h"

Vec *func_env;
Map *global_env;
Environment *local_vars;
int scoped_stack_space;
int max_scoped_stack_space;

// global

void sema_globals();
void sema_const(Node*);
void sema_const_aux(Node*);
void sema_const_array(Type*, Node*);

// Functions, statements and expressions

void sema_func(Func*);
void sema_block(Vec*, Func*);
void sema_stmt(Node*, Func*);
void sema_expr(Node*, Func*);
void sema_lval(Node*, Func*);
void sema_array(Type*, Node*, Func*);

// Helpers

bool assignable(Type*, Type*);
bool assignable_decl(Type*, Type*);
bool eq_type(Type*, Type*);
Func *find_func(Node *node);

// Global

void sema_globals() {
    int globals_len = vec_len(global_vars->values);
    for (int i = 0; i < globals_len; i++) {
        Node *g = vec_at(global_vars->values, i);
        for (int j = 0; j < i; j++) {
            Node *h = vec_at(global_vars->values, j);
            if (!strcmp(g->name, h->name))
                error_loc(g->loc, "duplicate global variable found");
        }
    }

    global_env = map_new();
    for (int i = 0; i < globals_len; i++) {
        Node *g = vec_at(global_vars->values, i);
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
        Node *lhs = node->lhs;
        Node *rhs = node->rhs;

        if (is_integer(lhs->type) && is_integer(rhs->type))
            node->type = binary_int_op_result(lhs->type, rhs->type);
        else if ((is_pointer_compat(lhs->type) && is_integer(rhs->type)) ||
                (is_integer(lhs->type) && is_pointer_compat(rhs->type)))
            node->type = is_pointer_compat(lhs->type) ? lhs->type : rhs->type;
        else
            error_loc(node->loc, "[semantic] unsupported addition in a global variable initialization");
        return;
    }

    if (node->kind == ND_SUB) {
        sema_const_aux(node->lhs);
        sema_const_aux(node->rhs);
        Node *lhs = node->lhs;
        Node *rhs = node->rhs;

        if (is_integer(lhs->type) && is_integer(rhs->type))
            node->type = binary_int_op_result(lhs->type, rhs->type);
        else if (is_pointer_compat(lhs->type) && is_integer(rhs->type))
            node->type = lhs->type;
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

void sema_func(Func *func) {
    int params_len = vec_len(func->params);
    // no duplicate parameter
    for (int i = 0; i < params_len; i++) {
        Node *pi = vec_at(func->params, i);
        for (int j = i+1; j < params_len; j++) {
            Node *pj = vec_at(func->params, j);
            if (!strcmp(pi->name, pj->name))
                error_loc(pi->loc, "%s: duplicate parameter `%s'", func->name, pi->name);
        }
    }

    vec_push(func_env, func);

    // block
    local_vars = env_new(NULL);
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

void sema_stmt(Node *node, Func *func) {
    if (node->kind == ND_VARDECL) {
        Node *lvar = map_find(local_vars->map, node->lhs->name);
        if (lvar != NULL)
            error_loc(node->loc, "[semantic] duplicate variable declaration");

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
        sema_expr(node->lhs, func);
        eq_type(node->lhs->type, func->ret_type);
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
        sema_expr(node->cond, func);
        sema_stmt(node->body, func);
        return;
    }
    if (node->kind == ND_FOR) {
        Vec *for_block = vec_new();
        if (node->lhs != NULL)
            vec_push(for_block, node->lhs);
        vec_push(for_block, node->cond);
        if (node->rhs != NULL)
            vec_push(for_block, node->rhs);
        vec_push(for_block, node->body);
        sema_block(for_block, func);
        return;
    }
    if (node->kind == ND_DOWHILE) {
        sema_stmt(node->body, func);
        sema_expr(node->cond, func);
        return;
    }
    if (node->kind == ND_BLOCK) {
        sema_block(node->block, func);
        return;
    }

    sema_expr(node, func);
}

void sema_expr(Node* node, Func *func) {
    // ND_NUM,
    if (node->kind == ND_NUM) {
        eq_type(type_int, node->type);
        return;
    }
    // ND_STRING
    if (node->kind == ND_STRING) {
        if (!eq_type(type_ptr_char, node->type))
            error_loc(node->loc, "[internal] string");
        return;
    }
    if (node->kind == ND_CHAR) {
        if (!eq_type(type_char, node->type))
            error_loc(node->loc, "[internal] char");
        return;
    }
    // ND_VAR,
    if (node->kind == ND_VAR) {
        Node *resolved_local = env_find(local_vars, node->name);
        if (resolved_local != NULL) {
            // node->type = resolved_local->type;
            *node = *resolved_local;
            return;
        }

        Node *resolved_global = map_find(func->global_vars, node->name);
        if (resolved_global == NULL)
            error_loc(node->loc, "[semantic] undefined variable");
        *node = *resolved_global;
        return;
    }
    // ND_SEQ
    if (node->kind == ND_SEQ) {
        sema_expr(node->lhs, func);
        sema_expr(node->rhs, func);
        node->type = node->rhs->type;
        return;
    }
    // ND_ASGN,
    if (node->kind == ND_ASGN) {
        sema_lval(node->lhs, func);
        sema_expr(node->rhs, func);
        if (!assignable(node->lhs->type, node->rhs->type))
            error_loc(node->loc, "[semantic] type mismatch in an assignment statment");
        node->type = node->lhs->type;
        return;
    }
    // ND_ADDR,
    if (node->kind == ND_ADDR) {
        sema_lval(node->lhs, func);
        node->type = ptr_of(node->lhs->type);
        return;
    }
    // ND_DEREF,
    if (node->kind == ND_DEREF) {
        sema_expr(node->lhs, func);
        if (!is_pointer_compat(node->lhs->type))
            error_loc(node->loc, "dereferencing non-pointer is not allowed");
        node->type = node->lhs->type->ptr_to;
        return;
    }
    // ND_SIZEOF
    if (node->kind == ND_SIZEOF) {
        sema_expr(node->lhs, func);
        node->type = type_int;
        node->val = type_size(node->lhs->type);
        return;
    }
    if (node->kind == ND_NEG) {
        sema_expr(node->lhs, func);
        node->type = type_int;
        return;
    }
    // ND_CALL,
    if (node->kind == ND_CALL) {
        Func *f = find_func(node);
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
    if (node->kind == ND_COND) {
        sema_expr(node->cond, func);
        sema_expr(node->lhs, func);
        sema_expr(node->rhs, func);

        if (!eq_type(node->lhs->type, node->rhs->type))
            error_loc(node->loc, "[semantic] type mismatch in a conditional expression");
        return;
    }
    if (node->kind == ND_ATTR) {
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

    sema_expr(node->lhs, func);
    sema_expr(node->rhs, func);
    Type* lty = node->lhs->type;
    Type* rty = node->rhs->type;
    // ND_ADD,
    if (node->kind == ND_ADD) {
        if (is_int(lty) && is_int(rty))
            node->type = type_int;
        else if (is_pointer_compat(lty) && is_int(rty))
            node->type = coerce_pointer(lty);
        else if (is_int(lty) && is_pointer_compat(rty))
            node->type = coerce_pointer(rty);
        else
            error_loc(node->loc, "unsupported addition");
        return;
    }
    // ND_SUB,
    if (node->kind == ND_SUB) {
        if (is_int(lty) && is_int(rty))
            node->type = type_int;
        else if (is_pointer_compat(lty) && is_int(rty))
            node->type = coerce_pointer(lty);
        else
            error_loc(node->loc, "unsupported subtraction");
        return;
    }
    // ND_MUL,
    if (node->kind == ND_MUL) {
        if (is_int(lty) && is_int(rty))
            node->type = type_int;
        else
            error_loc(node->loc, "unsupported multiplication");
        return;
    }
    // ND_DIV,
    if (node->kind == ND_DIV) {
        if (is_int(lty) && is_int(rty))
            node->type = type_int;
        else
            error_loc(node->loc, "unsupported division");
        return;
    }
    // ND_MOD
    if (node->kind == ND_MOD) {
        if ((node->type = binary_int_op_result(lty, rty)) != NULL)
            return;
        else
            error_loc(node->loc, "[semantic] unsupported modulo");
    }
    // ND_EQ,
    if (node->kind == ND_EQ) {
        node->type = type_int;
        return;
    }
    // ND_NEQ,
    if (node->kind == ND_NEQ) {
        node->type = type_int;
        return;
    }
    // ND_LT,
    if (node->kind == ND_LT) {
        node->type = type_int;
        return;
    }
    // ND_LTE,
    if (node->kind == ND_LTE) {
        node->type = type_int;
        return;
    }
    // ND_LAND
    if (node->kind == ND_LAND) {
        node->type = type_int;
        return;
    }
    // ND_LAND
    if (node->kind == ND_LOR) {
        node->type = type_int;
        return;
    }
    // ND_IOR
    if (node->kind == ND_IOR) {
        if ((node->type = binary_int_op_result(lty, rty)) != NULL)
            return;
        else
            error_loc(node->loc, "[semantic] type mismatch in an inclusive OR expression");
    }
    // ND_XOR
    if (node->kind == ND_XOR) {
        if ((node->type = binary_int_op_result(lty, rty)) != NULL)
            return;
        else
            error_loc(node->loc, "[semantic] type mismatch in an exclusive OR expression");
    }
    // ND_AND
    if (node->kind == ND_AND) {
        if ((node->type = binary_int_op_result(lty, rty)) != NULL)
            return;
        else
            error_loc(node->loc, "[semantic] type mismatch in an AND expression");
    }
    if (node->kind == ND_LSH) {
        if ((node->type = lty) != NULL)
            return;
        else
            error_loc(node->loc, "[semantic] type mismatch in a left shift expression");
    }
    if (node->kind == ND_RSH) {
        if ((node->type = lty) != NULL)
            return;
        else
            error_loc(node->loc, "[semantic] type mismatch in a right shift expression");
    }
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
        error_loc(node->loc, "[semantic] should be left value");
    sema_expr(node, func);
}

// Helpers

// TODO: Needs to be improved
bool assignable(Type *lhs, Type *rhs) {
    if (lhs == type_int)
        return rhs == type_int || rhs == type_char;
    if (lhs == type_char)
        return rhs == type_char || rhs == type_int;
    if (is_pointer_compat(lhs))
        return is_pointer_compat(rhs);
    if (lhs->ty == TY_STRUCT && rhs->ty == TY_STRUCT)
        return false;

    error("[internal] assignable: unsupported type appeared");
    return false;
}

bool eq_type(Type* t1, Type* t2) {
    if (t1->ty == TY_PTR && t2->ty == TY_PTR)
        return eq_type(t1->ptr_to, t2->ptr_to);
    else if (t1->ty == TY_ARRAY && t2->ty == TY_ARRAY)
        return eq_type(t1->ptr_to, t2->ptr_to);
    else if (t1->ty == TY_STRUCT && t2->ty == TY_STRUCT)
        return t1 == t2;
    else
        return t1->ty == t2->ty;
}

Func *find_func(Node *node) {
    int funcs_len = vec_len(func_env);
    for (int i = 0; i < funcs_len; i++) {
        Func *func = vec_at(func_env, i);
        if (!strcmp(func->name, node->name))
            return func;
    }
    return NULL;
}
