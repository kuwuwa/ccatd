#include "ccatd.h"

Type *type_int; Type *type_char;
Type *type_ptr_char;
Type *type_void;

Type *ptr_of(Type *ty) {
    Type *ret = calloc(1, sizeof(Type));
    ret->ty = TY_PTR;
    ret->ptr_to = ty;
    return ret;
}

Type *array_of(Type *ty, int len) {
    Type *ret = calloc(1, sizeof(Type));
    ret->ty = TY_ARRAY;
    ret->ptr_to = ty;
    ret->array_size = len;
    return ret;
}

Type *func_returns(Type *ty) {
    Type *ret = calloc(1, sizeof(Type));
    ret->ty = TY_FUNC;
    ret->ptr_to = ty;
    return ret;
}

int type_size(Type *t) {
    if (t->ty == TY_CHAR)
        return 1;
    if (t->ty == TY_INT || t->ty == TY_ENUM)
        return 4;
    if (t->ty == TY_PTR)
        return 8;
    if (t->ty == TY_ARRAY)
        return t->array_size * type_size(t->ptr_to);
    if (t->ty == TY_STRUCT) {
        if (t->strct->fields == NULL)
            error_loc(t->strct->loc, "[codegen] struct size not determined");
        int size = 0;
        int num_fields = vec_len(t->strct->fields);
        for (int i = 0; i < num_fields; i++) {
            Node *field = vec_at(t->strct->fields, i);
            size += type_size(field->type);
        }
        return size;
    }

    error("type_size: unsupported type");
    return -1;
}

bool is_int(Type *t) {
    return t->ty == TY_INT;
}

bool is_integer(Type *t) {
    return t->ty == TY_INT || t->ty == TY_CHAR || t->ty == TY_ENUM;
}

bool is_pointer(Type *t) {
    return t->ty == TY_PTR;
}

bool is_pointer_compat(Type *t) {
    return t->ty == TY_PTR || t->ty == TY_ARRAY;
}

bool is_enum(Type *t) {
    return t->ty == TY_ENUM;
}

bool is_func(Type *t) {
    return t->ty == TY_FUNC;
}

Type *coerce_pointer(Type *t) {
    if (t->ty == TY_PTR)
        return t;
    if (t->ty == TY_ARRAY)
        return ptr_of(t->ptr_to);

    error("coerce_pointer; unsupported type");
    return NULL;
}

Type *binary_int_op_result(Type *lt, Type *rt) {
    if (!is_integer(lt) || !is_integer(rt))
        return NULL;

    if (lt->ty == TY_CHAR && rt->ty == TY_CHAR)
        return type_char;

    return type_int;
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

