#include <stdio.h>

#include "ccatd.h"

void gen(Node*);
void gen_lval(Node*);

// generate

int label_num = 0;

void gen(Node *node) {
    if (node->kind == ND_NUM) {
        printf("  mov rax, %d\n", node->val);
        printf("  push %d\n", node->val);
        return;
    }

    if (node->kind == ND_LVAR) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->offset);
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    }

    if (node->kind == ND_ASGN) {
        if (node->lhs->kind != ND_LVAR)
            error("left hand side of an assignment should be a left value");

        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->lhs->offset);
        printf("  push rax\n");

        gen(node->rhs);

        printf("  pop rax\n"
               "  pop rdi\n"
               "  mov [rdi], rax\n"
               "  push rax\n");
       return;
    }

    if (node->kind == ND_RETURN) {
        gen(node->lhs);
        printf("  pop rax\n"
               "  mov rsp, rbp\n"
               "  pop rbp\n"
               "  ret\n");
        return;
    }

    if (node->kind == ND_IF) {
        gen(node->cond);
        printf("  pop rax\n"
               "  cmp rax, 0\n");
        if (node->rhs == NULL) {
            printf("  je .Lend%d\n", label_num);
            gen(node->lhs);
            printf(".Lend%d:\n", label_num);
        } else {
            printf("  je  .Lelse%d\n", label_num);
            gen(node->lhs);
            printf("  jmp .Lend%d\n", label_num);
            printf(".Lelse%d:\n", label_num);
            gen(node->rhs);
            printf(".Lend%d:\n", label_num);
        }

        label_num += 1;
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
                    error("should be unreachable");
            }
            printf("  movzb rax, al\n");
    }
    printf("  push rax\n");
}
