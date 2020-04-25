
#include "ccatd.h"

Type *type_int;

Type *ptr_of(Type *ty) {
    Type *ret = (Type*) calloc(1, sizeof(Type));
    ret->ptr_to = ty;
    return ret;
}

bool is_int(Type *t) {
    return t->ty == TY_INT;
}

bool is_ptr(Type *t) {
    return t->ty == TY_PTR;
}

