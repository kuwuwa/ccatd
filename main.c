#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ccatd.h"

bool is_expr(Node_kind kind);

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid number of argument(s)\n");
        return 1;
    }

    token = tokenize(argv[1]);
    locals = calloc(1, sizeof(Lvar));
    locals->next = NULL;
    locals->len = -1;
    locals->offset = 0;
    prog();

    printf(".intel_syntax noprefix\n"
           ".global main\n"
           "main:\n"
           "  push rbp\n"
           "  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", locals->offset);

    for (int i = 0; code[i]; i++) {
        gen(code[i]);
        if (is_expr(code[i]->kind))
            printf("  pop rax\n");
    }

    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
    return 0;
}

bool is_expr(Node_kind kind) {
    static Node_kind expr_kinds[] = {
        ND_NUM, ND_LVAR, ND_ASGN, ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_EQ, ND_NEQ, ND_LT, ND_LTE
    };
    for (int i = 0; i < sizeof(expr_kinds) / sizeof(Node_kind); i++)
        if (expr_kinds[i] == kind) return true;
    return false;
}

