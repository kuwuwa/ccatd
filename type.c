
#include "ccatd.h"

Type *type_int;

Type *ptr_of(Type *ty) {
    Type *ret = (Type*) calloc(1, sizeof(Type));
    ret->ty = TY_PTR;
    ret->ptr_to = ty;
    return ret;
}

Type *array_of(Type *ty, int len) {
    Type *ret = (Type*) calloc(1, sizeof(Type));
    ret->ty = TY_ARRAY; 
    ret->ptr_to = ty;
    ret->array_size = len;
    return ret;
}

int type_size(Type *t) {
    if (t->ty == TY_INT)
        return 4;
    if (t->ty == TY_PTR)
        return 8;
    if (t->ty == TY_ARRAY)
        return t->array_size * type_size(t->ptr_to);

    error("type_size: unsupported type");
    return -1;
}

bool is_int(Type *t) {
    return t->ty == TY_INT;
}

bool is_pointer_compat(Type *t) {
    return t->ty == TY_PTR || t->ty == TY_ARRAY;
}

Type *coerce_pointer(Type *t) {
    if (t->ty == TY_PTR)
        return t;
    if (t->ty == TY_ARRAY)
        return ptr_of(t->ptr_to);

    error("coerce_pointer; unsupported type");
    return NULL;
}

