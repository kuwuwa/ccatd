#include <string.h>

#include "ccatd.h"


void sema_globals();
void sema_const(Node*);
void sema_const_aux(Node*);
void sema_const_array(Type*, Node*);
void sema_func(Func*);
void sema_block(Vec*, Func*);
void sema_stmt(Node*, Func*, int scope_start);
void sema_expr(Node*);
void sema_lval(Node*);
void sema_array(Type*, Node*);

bool assignable(Type*, Type*);
bool assignable_decl(Type*, Type*);
bool eq_type(Type*, Type*);

Vec *func_env;
Vec *local_vars;
int scoped_stack_space;
int max_scoped_stack_space;

Node *find_lvar_sema(Node *node);
int index_of_lvar(Node *node);

Func *find_func(Node *node);
Type *ptr_of_array(Type* typ);

// ------------------------------------------------------------

void sema_globals() {
    int globals_len = vec_len(environment->globals);
    for (int i = 0; i < globals_len; i++) {
        Node *g = vec_at(environment->globals, i);
        for (int j = 0; j < i; j++) {
            Node *h = vec_at(environment->globals, j);
            if (g->len == h->len && memcmp(g->name, h->name, g->len) == 0)
                error_loc(g->loc, "duplicate global variable found");
        }
    }

    for (int i = 0; i < globals_len; i++) {
        Node *g = vec_at(environment->globals, i);
        if (g->kind != ND_GVAR)
            error_loc(g->loc, "[semantic] a global variable expected");

        if (g->rhs != NULL) {
            sema_const(g);
            if (!assignable(g->type, g->rhs->type))
                error_loc(g->loc, "[semantic] type mismatch in a global variable initialization");
        }
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
            error("[semantic] type mismatch in a global variable declaration");
        return;
    }
    if (node->kind == ND_STRING) {
        if (!eq_type(type_ptr_char, node->type))
            error("[semantic] type mismatch in a global variable initialization");
        return;
    }

    if (node->kind == ND_ADDR) {
        Node *e = node->lhs;
        if (e->kind != ND_GVAR)
            error_loc(e->loc, "[semantic] a left value expected");

        node->type = ptr_of(e->type);
        return;
    }

    if (node->kind == ND_GVAR) {
        if (!is_pointer_compat(node->type))
            error_loc(node->loc, "[semantic] unsupported type of variable in a global variable initialization");
        return;
    }

    if (node->kind == ND_ADD) {
        sema_const_aux(node->lhs);
        sema_const_aux(node->rhs);
        Node *lhs = node->lhs;
        Node *rhs = node->rhs;

        if (is_integer(lhs->type) && is_integer(rhs->type))
            node->type = (lhs->type->ty == TY_CHAR && rhs->type->ty == TY_CHAR)
                ? type_char : type_int;
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
            node->type = (lhs->type->ty == TY_CHAR && rhs->type->ty == TY_CHAR)
                ? type_char : type_int;
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

void sema_func(Func *func) {
    int params_len = vec_len(func->params);
    // no duplicate parameter
    for (int i = 0; i < params_len; i++) {
        Node *pi = vec_at(func->params, i);
        for (int j = i+1; j < params_len; j++) {
            Node *pj = vec_at(func->params, j);
            if (!memcmp(pi->name, pj->name, pi->len)) {
                error_loc(pi->loc, "%s: duplicate parameter `%s'", func->name, pi->name);
            }
        }
    }

    vec_push(func_env, func);

    // block
    local_vars = vec_new();
    for (int i = 0; i < params_len; i++) {
        Node* param = vec_at(func->params, i);
        param->val = 8 * (i + 1);
        vec_push(local_vars, param);
    }
    max_scoped_stack_space = scoped_stack_space = 8 * params_len;

    sema_block(func->block, func);
    func->offset = max_scoped_stack_space;
}

void sema_block(Vec *block, Func *func) {
    int scope_start = vec_len(local_vars);
    int block_len = vec_len(block);
    for (int i = 0; i < block_len; i++)
        sema_stmt(vec_at(block, i), func, scope_start);

    while (vec_len(local_vars) > scope_start) {
        Node *var = vec_pop(local_vars);
        scoped_stack_space -= type_size(var->type);
    }
}

void sema_stmt(Node *node, Func *func, int scope_start) {
    if (node->kind == ND_VARDECL) {
        int idx = index_of_lvar(node->lhs);
        if (scope_start <= idx && idx < vec_len(local_vars))
            error_loc(node->loc, "duplicate variable declaration");

        if (node->rhs != NULL) {
            if (node->lhs->type->ty == TY_ARRAY) {
                if (node->lhs->type->ptr_to->ty == TY_CHAR && node->rhs->kind == ND_STRING)
                    ;
                else if (node->rhs->kind == ND_ARRAY)
                    sema_array(node->lhs->type, node->rhs);
                else
                    error_loc(node->loc, "[semantic] unsupported array initialization");
            } else {
                sema_expr(node->rhs);
                if (!assignable(node->lhs->type, node->rhs->type))
                    error_loc(node->loc, "[semantic] type mismatch in a variable declaration");
            }
        }

        node->lhs->val = 8 + scoped_stack_space;
        vec_push(local_vars, node->lhs);
        scoped_stack_space += type_size(node->lhs->type);
        max_scoped_stack_space = scoped_stack_space > max_scoped_stack_space
            ? scoped_stack_space
            : max_scoped_stack_space;

        node->type = node->lhs->type;
        return;
    }
    if (node->kind == ND_RETURN) {
        sema_expr(node->lhs);
        eq_type(node->lhs->type, func->ret_type);
        return;
    }
    if (node->kind == ND_IF) {
        sema_expr(node->cond);
        sema_stmt(node->lhs, func, scope_start);
        if (node->rhs != NULL)
            sema_stmt(node->rhs, func, scope_start);
        return;
    }
    if (node->kind == ND_WHILE) {
        sema_expr(node->cond);
        sema_stmt(node->body, func, scope_start);
        return;
    }
    if (node->kind == ND_FOR) {
        sema_expr(node->cond);
        sema_expr(node->lhs);
        sema_expr(node->rhs);
        sema_stmt(node->body, func, scope_start);
        return;
    }
    if (node->kind == ND_BLOCK) {
        sema_block(node->block, func);
        return; 
    }

    sema_expr(node);
}

void sema_expr(Node* node) {
    // ND_NUM,
    if (node->kind == ND_NUM) {
        eq_type(type_int, node->type);
        return;
    }
    // ND_NUM
    if (node->kind == ND_STRING) {
        if (!eq_type(type_ptr_char, node->type))
            error_loc(node->loc, "[internal] string");
        return;
    }
    // ND_LVAR,
    if (node->kind == ND_LVAR) {
        Node *resolved = find_lvar_sema(node);

        if (resolved == NULL)
            error_loc(node->loc, "undefined variable");
        node->type = resolved->type;
        return;
    }
    // ND_ASGN,
    if (node->kind == ND_ASGN) {
        sema_lval(node->lhs);
        sema_expr(node->rhs);
        if (!assignable(node->lhs->type, node->rhs->type))
            error_loc(node->loc, "[semantic] type mismatch in an assignment statment");
        node->type = node->lhs->type;
        return;
    }
    // ND_ADDR,
    if (node->kind == ND_ADDR) {
        sema_lval(node->lhs);
        node->type = ptr_of(node->lhs->type);
        return;
    }
    // ND_DEREF,
    if (node->kind == ND_DEREF) {
        sema_expr(node->lhs);
        if (!is_pointer_compat(node->lhs->type))
            error_loc(node->loc, "dereferencing non-pointer is not allowed");
        node->type = node->lhs->type->ptr_to;
        return;
    }
    // ND_SIZEOF
    if (node->kind == ND_SIZEOF) {
        sema_expr(node->lhs);
        node->type = type_int;
        node->val = type_size(node->lhs->type);
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
            sema_expr(vec_at(node->block, i));

        node->type = f->ret_type;
        return;
    }
    if (node->kind == ND_GVAR)
        return;

    sema_expr(node->lhs);
    sema_expr(node->rhs);
    Type* lty = node->lhs->type;
    Type* rty = node->rhs->type;
    // ND_ADD,
    if (node->kind == ND_ADD) {
        if (is_int(lty) && is_int(rty)) {
            node->type = type_int;
        } else if (is_pointer_compat(lty) && is_int(rty)) {
            node->type = coerce_pointer(lty);
        } else if (is_int(lty) && is_pointer_compat(rty)) {
            node->type = coerce_pointer(rty);
        } else {
            error_loc(node->loc, "unsupported addition");
        }
        return;
    }
    // ND_SUB,
    if (node->kind == ND_SUB) {
        if (is_int(lty) && is_int(rty)) {
            node->type = type_int;
        } else if (is_pointer_compat(lty) && is_int(rty)) {
            node->type = coerce_pointer(lty);
        } else {
            error_loc(node->loc, "unsupported subtraction");
        }
        return;
    }
    // ND_MUL,
    if (node->kind == ND_MUL) {
        if (is_int(lty) && is_int(rty)) {
            node->type = type_int;
        } else {
            error_loc(node->loc, "unsupported multiplication");
        }
        return;
    }
    // ND_DIV,
    if (node->kind == ND_DIV) {
        if (is_int(lty) && is_int(rty)) {
            node->type = type_int;
        } else {
            error_loc(node->loc, "unsupported division");
        }
        return;
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
    error_loc(node->loc, "unsupported feature");
}

void sema_array(Type* ty, Node* arr) {
    int array_len = vec_len(arr->block);
    if (array_len > ty->array_size) {
        error_loc(arr->loc, "[semantic] too long array");
    }

    Type *elem_type = ty->ptr_to;
    for (int i = 0; i < array_len; i++) {
        Node *e = vec_at(arr->block, i);
        sema_expr(e);
        if (!eq_type(elem_type, e->type))
            error_loc(e->loc, "[semantic] type mismatch in array");
    }

    arr->type = ty;
}

void sema_lval(Node *node) {
    if (node->kind == ND_LVAR || node->kind == ND_GVAR || node->kind == ND_DEREF) {
        sema_expr(node);
        return;
    }
    error_loc(node->loc, "[semantic] should be left value");
}

bool assignable(Type *lhs, Type *rhs) {
    if (lhs == type_int)
        return rhs == type_int || rhs == type_char;
    if (lhs == type_char)
        return rhs == type_char || rhs == type_int;
    if (is_pointer_compat(lhs))
        return is_pointer_compat(rhs) && eq_type(lhs->ptr_to, rhs->ptr_to);

    error("[internal] assignable: unsupported type appeared");
    return false;
}

bool eq_type(Type* t1, Type* t2) {
    if (t1->ty == TY_PTR && t2->ty == TY_PTR) {
        return eq_type(t1->ptr_to, t2->ptr_to);
    } else if (t1->ty == TY_ARRAY && t2->ty == TY_ARRAY) {
        return eq_type(t1->ptr_to, t2->ptr_to);
    } else {
        return t1->ty == t2->ty;
    }
}

Node *find_lvar_sema(Node *target) {
    int idx = index_of_lvar(target);
    return (0 <= idx && idx < vec_len(local_vars)) ? vec_at(local_vars, idx) : NULL;
}

int index_of_lvar(Node *target) {
    int local_vars_len = vec_len(local_vars);
    int ret = local_vars_len;
    for (int i = 0; i < local_vars_len; i++) {
        Node *var = vec_at(local_vars, i);
        if (var->len == target->len &&
                memcmp(var->name, target->name, var->len) == 0)
            ret = i;
    }
    return ret;
}

Func *find_func(Node *node) {
    int funcs_len = vec_len(func_env);
    for (int i = 0; i < funcs_len; i++) {
        Func *func = vec_at(func_env, i);
        if (strlen(func->name) == strlen(node->name) && !strcmp(func->name, node->name))
            return func;
    }
    return NULL;
}

Type *ptr_of_array(Type *typ) {
    if (typ->ty == TY_ARRAY)
        return ptr_of(typ->ptr_to);
    return typ;
}
