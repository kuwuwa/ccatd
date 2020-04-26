
#include "ccatd.h"

Type *type_int;

Type *ptr_of(Type *ty) {
    Type *ret = (Type*) calloc(1, sizeof(Type));
    ret->ty = TY_PTR;
    ret->ptr_to = ty;
    return ret;
}

int type_size(Type *t) {
    return t->ty == TY_INT ? 4 : 8;
}

bool is_int(Type *t) {
    return t->ty == TY_INT;
}

bool is_ptr(Type *t) {
    return t->ty == TY_PTR;
}

