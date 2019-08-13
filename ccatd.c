#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid number of argument(s)\n");
        return 1;
    }

    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");
    printf("  mov rax, %s\n", argv[1]);
    printf("  ret\n");
    return 0;
}
