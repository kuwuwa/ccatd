#include <stdio.h>

#include "ccatd.h"

char *arg_regs[6] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9", };

void gen(Node*);
void gen_stmt(Node*);
void gen_func(Func*);
bool is_expr(Node_kind);
void nputs(char *str, int n);

// generate

int label_num = 0;
int stack_depth = 0;

void gen(Node *node) {
    if (node->kind == ND_NUM) {
        printf("  mov rax, %d\n", node->val);
        printf("  push %d\n", node->val);
        stack_depth += 8;
        return;
    }

    if (node->kind == ND_LVAR) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->val);
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        stack_depth += 8;
        return;
    }

    if (node->kind == ND_ASGN) {
        if (node->lhs->kind != ND_LVAR)
            error("left hand side of an assignment should be a left value");

        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->lhs->val);
        printf("  push rax\n");
        stack_depth += 8;

        gen(node->rhs);

        printf("  pop rax\n"
               "  pop rdi\n"
               "  mov [rdi], rax\n"
               "  push rax\n");
        stack_depth += 8;
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
            printf("  je .Lend_if%d\n", label_num);
            gen_stmt(node->lhs);
            printf(".Lend_if%d:\n", label_num);
        } else {
            printf("  je  .Lelse%d\n", label_num);
            gen_stmt(node->lhs);
            printf("  jmp .Lend_if%d\n", label_num);
            printf(".Lelse%d:\n", label_num);
            gen_stmt(node->rhs);
            printf(".Lend_if%d:\n", label_num);
        }

        label_num += 1;
        return;
    }

    if (node->kind == ND_WHILE) {
        printf(".Lwhile%d:\n", label_num);
        gen(node->cond);
        printf("  pop rax\n"
               "  cmp rax, 0\n");
        printf("  je .Lend_while%d\n", label_num);
        gen_stmt(node->body);
        printf("  jmp .Lwhile%d\n", label_num);
        printf(".Lend_while%d:\n", label_num);

        label_num += 1;
        return;
    }

    if (node->kind == ND_FOR) {
        gen_stmt(node->lhs);
        printf(".Lfor%d:\n", label_num);
        gen(node->cond);
        printf("  pop rax\n"
               "  cmp rax, 0\n");
        printf("  je .Lend_for%d\n", label_num);
        gen_stmt(node->body);
        gen_stmt(node->rhs);
        printf("  jmp .Lfor%d\n", label_num);
        printf(".Lend_for%d:\n", label_num);
        return;
    }

    if (node->kind == ND_BLOCK) {
        int len = vec_len(node->block);
        for (int i = 0; i < len; i++)
            gen_stmt(vec_at(node->block, i));
        return;
    }

    if (node->kind == ND_CALL) {
        int arg_len = vec_len(node->block);
        for (int i = 0; i < arg_len; i++)
            gen(vec_at(node->block, i));
        for (int i = arg_len-1; i >= 0; i--)
            printf("  pop %s\n", arg_regs[i]);
        stack_depth -= 8 * arg_len;
        if (stack_depth % 16 != 0)
            printf("  sub rsp, 8\n"); // 16-byte boundary
        printf("  and rsp, -16\n");
        printf("  call ");
        nputs(node->name, node->len);
        printf("\n");
        if (stack_depth % 16 != 0)
            printf("  add rsp, 8\n"); // 16-byte boundary
        printf("  push rax\n");
        stack_depth += 8;
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    stack_depth -= 16;

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
                    break;
                default:
                    error("should be unreachable");
            }
            printf("  movzb rax, al\n");
    }
    printf("  push rax\n");
    stack_depth += 8;
}

void gen_stmt(Node* node) {
    gen(node);
    if (is_expr(node->kind)) {
        printf("  pop rax # throw away from stack\n");
        stack_depth -= 8;
    }
}

void gen_func(Func* func) {
    nputs(func->name, func->len);
    printf(":\n");
    printf("  push rbp\n"
           "  mov rbp, rsp\n");
    for (int i = 0; i < vec_len(func->params); i++)
        printf("  push %s\n", arg_regs[i]);
    printf("  sub rsp, %d\n", func->offset - 8 * vec_len(func->params));
    stack_depth = func->offset;
    for (int i = 0; i < vec_len(func->block); i++)
        gen_stmt(vec_at(func->block, i));
    printf("  mov rsp, rbp\n"
           "  pop rbp\n"
           "  ret\n");
}

bool is_expr(Node_kind kind) {
    static Node_kind expr_kinds[] = {
        ND_NUM, ND_LVAR, ND_ASGN, ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_EQ, ND_NEQ, ND_LT, ND_LTE,
        ND_CALL,
    };
    for (int i = 0; i < sizeof(expr_kinds) / sizeof(Node_kind); i++)
        if (expr_kinds[i] == kind) return true;
    return false;
}

void nputs(char *str, int n) {
    for (int i = 0; i < n; i++)
        putc(str[i], stdout);
}
