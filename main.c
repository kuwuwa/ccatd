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

    locals = calloc(1, sizeof(Lvar));
    locals->next = NULL;
    locals->len = -1;
    locals->offset = 0;

    code = prog();

    printf(".intel_syntax noprefix\n"
           ".global main\n"
           "main:\n"
           "  push rbp\n"
           "  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", locals->offset);

    gen_stmt(code);

    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
    return 0;
}

