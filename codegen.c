#include <stdio.h>

#include "ccatd.h"

// generate
void gen(Node *node) {
    if (node->kind == ND_NUM) {
        printf("  push %d\n", node->val);
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind) {
        case ND_ADD:
            printf("  add rax, rdi\n");
            break;
        case ND_SUB:
            printf("  sub rax, rdi\n");
            break;
        case ND_MUL:
            printf("  imul rax, rdi\n");
            break;
        case ND_DIV:
            printf("  cqo\nidiv rdi\n");
            break;
        default:
            printf("  cmp rax, rdi\n");
            switch (node->kind) {
                case ND_EQ:
                    printf("  sete al\n");
                    break;
                case ND_NEQ:
                    printf("  setne al\n");
                    break;
                case ND_LT:
                    printf("  setl al\n");
                    break;
                case ND_LTE:
                    printf("  setle al\n");
                default:
                    break;
            }
            printf("  movzb rax, al\n");
    }

    printf("  push rax\n");
}

