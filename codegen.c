#include <stdio.h>
#include <stdlib.h>

#include "ccatd.h"

char *arg_regs64[6] = {"rdi", "rsi", "rdx", "rcx", "r8",  "r9"};
char *arg_regs32[6] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};

void gen(Node*);
void gen_stmt(Node*);
void gen_func(Func*);
void gen_lval(Node*);
bool is_expr(Node_kind);
void gen_coeff_ptr(Type*, Type*);
char *ax_of_type(Type*);
char *di_of_type(Type*);

// generate

int label_num = 0;
int stack_depth = 0;

void gen(Node *node) {
    if (node->kind == ND_NUM) {
        // assuming `int'
        printf("  mov eax, %d\n", node->val);
        printf("  push %d\n", node->val);
        stack_depth += 8;
        return;
    }

    if (node->kind == ND_LVAR) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->val);

        char *ax = ax_of_type(node->type);
        printf("  mov %s, [rax]\n", ax);
        printf("  push rax\n");
        stack_depth += 8;
        return;
    }

    if (node->kind == ND_ASGN) {
        gen_lval(node->lhs);
        gen(node->rhs);
        char *ax = ax_of_type(node->type);
        printf("  pop rax\n"
               "  pop rdi\n");
        printf("  mov [rdi], %s\n", ax);
        printf("  push rax\n");
        stack_depth += 8;
        return;
    }

    if (node->kind == ND_CALL) {
        int arg_len = vec_len(node->block);
        int revert_stack_depth = stack_depth;
        for (int i = 0; i < arg_len; i++)
            gen(vec_at(node->block, i));
        for (int i = arg_len-1; i >= 0; i--) {
            printf("  pop %s\n", arg_regs64[i]);
        }
        stack_depth = revert_stack_depth;
        if (stack_depth % 16 != 0)
            printf("  sub rsp, 8\n"); // 16-byte boundary
        printf("  call ");
        fnputs(stdout, node->name, node->len);
        printf("\n");
        if (stack_depth % 16 != 0)
            printf("  add rsp, 8\n"); // 16-byte boundary

        printf("  push rax\n");
        stack_depth += 8;
        return;
    }

    if (node->kind == ND_ADDR) {
        gen_lval(node->lhs);
        return;
    }

    if (node->kind == ND_DEREF) {
        gen(node->lhs);
        printf("  pop rax\n"
               "  mov rax, [rax]\n"
               "  push rax\n");
        return;
    }

    if (node->kind == ND_VARDECL) {
        gen_lval(node->lhs);
        gen(node->rhs);
        printf("  pop rax\n");
        printf("  pop rdi\n");
        char *ax = ax_of_type(node->type);
        printf("  mov [rdi], %s\n", ax);
        printf("  push rax\n");
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

        label_num += 1;
        return;
    }

    if (node->kind == ND_BLOCK) {
        int len = vec_len(node->block);
        for (int i = 0; i < len; i++)
            gen_stmt(vec_at(node->block, i));
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    stack_depth -= 16;

    switch (node->kind) {
        case ND_ADD:
            gen_coeff_ptr(node->lhs->type, node->rhs->type);
            printf("  add rax, rdi\n");
            break;
        case ND_SUB:
            gen_coeff_ptr(node->lhs->type, node->rhs->type);
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
    fnputs(stdout, func->name, func->len);
    printf(":\n");
    printf("  push rbp\n"
           "  mov rbp, rsp\n");
    for (int i = 0; i < vec_len(func->params); i++)
        printf("  push %s\n", arg_regs64[i]);
    int local_vars_space = func->offset - 8 * vec_len(func->params);
    printf("  sub rsp, %d\n", 8 + local_vars_space);
    stack_depth = func->offset;
    for (int i = 0; i < vec_len(func->block); i++)
        gen_stmt(vec_at(func->block, i));
    printf("  mov rsp, rbp\n"
           "  pop rbp\n"
           "  ret\n");
}

void gen_lval(Node *node) {
    if (node->kind == ND_LVAR) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->val);
        printf("  push rax\n");
        stack_depth += 8;
        return;
    } else if (node->kind == ND_DEREF) {
        gen(node->lhs);
        return;
    } else if (node->kind == ND_NUM) {
        gen(node);
        return;
    }
    error("term should be a left value");
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

void gen_coeff_ptr(Type* lt /* rax */, Type* rt /* rdi */) {
    if (lt->ty == TY_INT && rt->ty == TY_INT) {
    } else if (lt->ty == TY_INT) {
        printf("  imul rax, %d\n", (rt->ptr_to->ty == TY_INT ? 4 : 8));
    } else if (rt->ty == TY_INT) {
        printf("  imul rdi, %d\n", (lt->ptr_to->ty == TY_INT ? 4 : 8));
    } else {
        error("addition/subtraction of two pointers is not allowed");
    }
}


char *ax_of_type(Type *type) {
    return type_size(type) == 8 ? "rax" : "eax";
}

char *di_of_type(Type *type) {
    return type_size(type) == 8 ? "rdi" : "edi";
}
