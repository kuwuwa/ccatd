#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

void append_type_param(Vec *params, Type* t) {
    Node *node = (Node*) calloc(1, sizeof(Node));
    node->type = t;
    node->kind = ND_LVAR;
    vec_push(params, node);
}

void push_function(char *name, Type **arg_types, int argc, Type *ret_type) {
    Func *func = (Func*) calloc(1, sizeof(Func));
    func->name = name;
    func->len = strlen(name);
    func->params = vec_new();
    for (int i = 0; i < argc; i++)
        append_type_param(func->params, arg_types[i]);
    func->ret_type = ret_type;

    vec_push(func_env, func);
}

void init() {
    type_int = calloc(1, sizeof(Type));
    type_int->ty = TY_INT;
    type_int->ptr_to = NULL;

    type_char = calloc(1, sizeof(Type));
    type_char->ty = TY_CHAR;
    type_char->ptr_to = NULL;

    func_env = vec_new();
    push_function(
            "foo",
            NULL,
            0,
            type_int // TODO: void
    );

    Type* bar_args[3] = {type_int, type_int, type_int};
    push_function(
            "bar",
            bar_args,
            3,
            type_int
    );

    Type *alloc4_args[5] =
        {ptr_of(ptr_of(type_int)), type_int, type_int, type_int, type_int};
    push_function(
            "alloc4",
            alloc4_args,
            5,
            ptr_of(type_int)
    );

    Type *print_args[1] = {ptr_of(type_char)};
    push_function(
            "print",
            print_args,
            1,
            type_int // void
    );
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

    gen_globals();

    int len = vec_len(environment->functions);

    printf("  .text\n");
    for (int i = 0; i < len; i++)
        gen_func(vec_at(environment->functions, i));
    return 0;
}

