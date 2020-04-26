#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ccatd.h"

void append_type_param(Vec *params, Type* t) {
    Node *node = (Node*) calloc(1, sizeof(Node));
    node->type = t;
    node->kind = ND_LVAR;
    vec_push(params, node);
}

Func *make_function(char *name, int len, Type **arg_types, int argc, Type *ret_type) {
    Func *func = (Func*) calloc(1, sizeof(Func));
    func->name = name;
    func->len = len;
    func->params = vec_new();
    for (int i = 0; i < argc; i++)
        append_type_param(func->params, arg_types[i]);
    func->ret_type = ret_type;
    return func;
}

void init() {
    type_int = calloc(1, sizeof(Type));
    type_int->ty = TY_INT;
    type_int->ptr_to = NULL;

    func_env = vec_new();
    Func *foo = make_function(
            "foo", 3,
            NULL,
            0,
            type_int // TODO: void
    );

    Type* bar_args[3] = {type_int, type_int, type_int};
    Func *bar = make_function(
            "bar", 3,
            bar_args,
            3,
            type_int
    );

    Type *alloc4_args[5] =
        {ptr_of(ptr_of(type_int)), type_int, type_int, type_int, type_int};
    Func *alloc4_init = make_function(
            "alloc4", 6,
            alloc4_args,
            5,
            ptr_of(type_int)
    );

    vec_push(func_env, foo);
    vec_push(func_env, bar);
    vec_push(func_env, alloc4_init);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid number of argument(s)\n");
        return 1;
    }

    tokens = tokenize(argv[1]);

    init();
    parse();

    for (int i = 0; i < vec_len(environment->functions); i++) {
        sema_func(vec_at(environment->functions, i));
    }

    printf("  .intel_syntax noprefix\n"
           "  .globl main\n");

    int len = vec_len(environment->functions);

    printf("  .text\n");
    for (int i = 0; i < len; i++)
        gen_func(vec_at(environment->functions, i));

    gen_globals();
    return 0;
}

