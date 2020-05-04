#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
    func->params = vec_new();
    for (int i = 0; i < argc; i++)
        append_type_param(func->params, arg_types[i]);
    func->ret_type = ret_type;

    vec_push(func_env, func);
}

void init() {
    // type

    type_int = calloc(1, sizeof(Type));
    type_int->ty = TY_INT;
    type_int->ptr_to = NULL;

    type_char = calloc(1, sizeof(Type));
    type_char->ty = TY_CHAR;
    type_char->ptr_to = NULL;

    type_ptr_char = ptr_of(type_char);

    // parse

    environment = calloc(1, sizeof(Environment));
    environment->functions = vec_new();
    environment->globals = vec_new();
    environment->string_literals = vec_new();
    environment->structs = vec_new();

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

    Type *assert_equals_args[2] = {type_int, type_int};
    push_function(
            "assert_equals",
            assert_equals_args,
            2,
            type_int // void
    );
}

char *read_file(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        error("cannot open %s: %d\n", path, strerror(errno));

    if (fseek(fp, 0, SEEK_END) == -1)
        error("%s: fseek: %s", path, strerror(errno));

    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) == -1)
        error("%s: fseek: %s", path, strerror(errno));

    char *buf = calloc(1, size + 2);
    fread(buf, size, 1, fp);

    if (size == 0 || buf[size - 1] != '\n')
        buf[size++] = '\n';
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid number of argument(s)\n");
        return 1;
    }
    init();

    char *code = read_file(argv[1]);
    tokenize(code);
    parse();

    sema_globals();

    for (int i = 0; i < vec_len(environment->functions); i++) {
        sema_func(vec_at(environment->functions, i));
    }

    printf("  .intel_syntax noprefix\n"
           "  .globl main\n");

    gen_globals();

    int len = vec_len(environment->functions);
    for (int i = 0; i < len; i++) {
        gen_func(vec_at(environment->functions, i));
    }
    return 0;
}

