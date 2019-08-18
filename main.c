#include <stdio.h>

#include "ccatd.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid number of argument(s)\n");
        return 1;
    }

    token = tokenize(argv[1]);
    prog();

    printf(".intel_syntax noprefix\n"
           ".global main\n"
           "main:\n"
           "  push rbp\n"
           "  mov rbp, rsp\n"
           "  sub rsp, 208\n");

    for (int i = 0; code[i]; i++) {
        gen(code[i]);
        printf("  pop rax\n");
    }
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
    return 0;
}