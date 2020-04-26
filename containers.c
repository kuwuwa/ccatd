#include <stdlib.h>

#include "ccatd.h"

struct Vector {
    int len;
    int cap;
    void **data;
};

Vec *vec_new() {
    Vec *vec = calloc(1, sizeof(Vec));
    vec->len = 0;
    vec->cap = 8;
    vec->data = calloc(8, sizeof(Node*));
    return vec;
}

int vec_len(Vec* vec) {
    return vec->len;
}

void vec_push(Vec *vec, void *node) {
    if (vec->len == vec->cap) {
        vec->cap *= 2;
        vec->data = realloc(vec->data, vec->cap * sizeof(void*));
    }
    vec->data[vec->len++] = node;
}

void *vec_pop(Vec *vec) {
    if (vec->len == 0)
        return NULL;

    void * ptr = vec->data[vec->len-1];
    vec->data[vec->len--] = NULL;
    return ptr;
}

void *vec_at(Vec *vec, int idx) {
    if (idx < 0 || vec->len <= idx)
        return NULL;
    return vec->data[idx];
}
