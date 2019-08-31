#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ccatd.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid number of argument(s)\n");
        return 1;
    }

    tokens = tokenize(argv[1]);

    code = parse();

    type_int = calloc(1, sizeof(Type));
    type_int->ty = TY_INT;

    printf(".intel_syntax noprefix\n"
           ".global main\n");

    int len = vec_len(code);
    for (int i = 0; i < len; i++)
        gen_func(vec_at(code, i));
    return 0;
}

