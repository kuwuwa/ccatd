#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

char *arg_regs64[6] = {"rdi", "rsi", "rdx", "rcx", "r8",  "r9"};

void gen_globals();
void gen_const(Node*);
Node *gen_const_calc(Node*);
void gen(Node*);
void gen_stmt(Node*);
void gen_func(Func*);
void gen_lval(Node*);
bool is_expr(Node_kind);
void gen_coeff_ptr(Type*, Type*);
char *ax_of_type(Type*);
char *di_of_type(Type*);
char *word_of_type(Type*);

// generate global variables

void gen_globals() {
    printf("  .data\n");
    for (int i = 0; i < vec_len(environment->globals); i++) {
        Node *global = vec_at(environment->globals, i);

        printf("  .globl ");
        fnputs(stdout, global->name, global->len);
        printf("\n");
    }

    for (int i = 0; i < vec_len(environment->globals); i++) {
        Node *global = vec_at(environment->globals, i);
        fnputs(stdout, global->name, global->len);
        printf(":\n");
        if (global->rhs == NULL)
            printf("  .zero %d\n", type_size(global->type));
        else {
            Node *rhs = gen_const_calc(global->rhs);
            gen_const(rhs);
        }
    }
    printf("  .text\n");
    for (int i = 0; i < vec_len(environment->string_literals); i++) {
        String *str = vec_at(environment->string_literals, i);
        printf(".LC%d:\n  .string \"", i);
        fnputs(stdout, str->ptr, str->len);
        printf("\"\n");
    }
}

void gen_const(Node *node) {
    if (node->kind == ND_NUM && node->type == type_int) {
        printf("  .long %d\n", node->val);
        return;
    }
    if (node->kind == ND_ADDR) {
        Node *var = node->lhs;
        printf("  .quad ");
        fnputs(stdout, var->name, var->len);
        printf("\n");
        return;
    }
    if (node->kind == ND_GVAR) {
        printf("  .quad ");
        fnputs(stdout, node->name, node->len);
        printf("\n");
        return;
    }
    if (node->kind == ND_STRING) {
        int len = vec_len(environment->string_literals);
        for (int i = 0; i < len; i++) {
            String *str = vec_at(environment->string_literals, i);
            if (node->len == str->len && !memcmp(str->ptr, node->name, str->len)) {
                printf("  .quad .LC%d\n", i);
                return;
            }
        }
    }
    if (node->kind == ND_ARRAY) {
        int arr_len = vec_len(node->block);
        for (int i = 0; i < arr_len; i++)
            gen_const(vec_at(node->block, i));
        return;
    }
    if (node->kind == ND_ADD) {
        Node *var = node->kind == ND_ADDR ? node->lhs->lhs : node->lhs;
        Node *offset = node->rhs;

        printf("  .quad ");
        fnputs(stdout, var->name, var->len);
        printf(" + %d\n", offset->val * type_size(offset->type));
        return;
    }
    if (node->kind == ND_SUB) {
        Node *var = node->lhs->lhs;
        Node *offset = node->rhs;

        printf("  .quad ");
        fnputs(stdout, var->name, var->len);
        printf(" - %d\n", offset->val * type_size(offset->type));
        return;
    }

    error_loc(node->loc, "[codegen] unsupported expression in a global variable declaration");
}

Node *gen_const_calc(Node *node) {
    if (node->kind == ND_NUM)
        return node;
    if (node->kind == ND_ADDR)
        return node;
    if (node->kind == ND_GVAR)
        return node;
    if (node->kind == ND_STRING)
        return node;
    if (node->kind == ND_ARRAY)
        return node;

    if (node->kind == ND_ADD) {
        gen_const_calc(node->lhs);
        gen_const_calc(node->rhs);
        if (is_integer(node->lhs->type) && is_integer(node->rhs->type)) {
            node->type = ND_NUM;
            node->val = node->lhs->val + node->rhs->val;
        } else {
            if (is_pointer_compat(node->rhs->type)) {
                Node *tmp = node->rhs;
                node->rhs = node->lhs;
                node->lhs = tmp;
            }

            if (node->lhs->kind == ND_ADD) {
                Node *new_rhs = new_node_num(node->lhs->rhs->val + node->rhs->val, node->loc);
                node->lhs = node->lhs->lhs;
                node->rhs = new_rhs;
            } else if (node->lhs->kind == ND_SUB) {
                Node *new_rhs = new_node_num(-node->lhs->rhs->val + node->rhs->val, node->loc);
                node->lhs = node->lhs->lhs;
                node->rhs = new_rhs;
            }
        }
        return node;
    }
    if (node->kind == ND_SUB) {
        gen_const_calc(node->lhs);
        gen_const_calc(node->rhs);
        if (is_integer(node->lhs->type) && is_integer(node->rhs->type)) {
            node->type = ND_NUM;
            node->val = node->lhs->val - node->rhs->val;
        } else {
            // is_pointer(node->lhs->type) && !is_pointer(node->rhs->type)

            if (node->lhs->kind == ND_ADD) {
                Node *new_rhs = new_node_num(node->lhs->rhs->val - node->rhs->val, node->loc);
                node->lhs = node->lhs->lhs;
                node->rhs = new_rhs;
            } else if (node->lhs->kind == ND_SUB) {
                Node *new_rhs = new_node_num(-node->lhs->rhs->val - node->rhs->val, node->loc);
                node->lhs = node->lhs->lhs;
                node->rhs = new_rhs;
            }
        }
        return node;
    }

    error_loc(node->loc, "[codegen] unsupported expression in a global variable declaration");
    return NULL;
}

// generate function

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
    if (node->kind == ND_STRING) {
        for (int i = 0; i < vec_len(environment->string_literals); i++) {
            String *str = vec_at(environment->string_literals, i);
            if (node->len == str->len && !memcmp(node->name, str->ptr, str->len)) {
                printf("  mov rax, OFFSET FLAT:.LC%d\n", i);
                printf("  push rax\n");
                return;
            }
        }
    }

    if (node->kind == ND_LVAR) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->val);

        if (node->type->ty == TY_ARRAY) {
            // pass; put an beginning address of given array
        } else {
            int size = type_size(node->type);
            if (size == 1) {
                printf("  movsx eax, BYTE PTR [rax]\n");
            } else if (size == 4) {
                printf("  mov eax, [rax]\n");
            } else { /* size == 8 */
                printf("  mov rax, [rax]\n");
            }
        }
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
        int diff = (16 - stack_depth % 16) % 16;
        if (diff != 0)
            printf("  sub rsp, %d\n", diff); // 16-bit boundary
        printf("  call ");
        fnputs(stdout, node->name, node->len);
        printf("\n");
        if (diff != 0)
            printf("  add rsp, %d\n", diff); // 16-bit boundary

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
        printf("  pop rax\n");
        int size = type_size(node->type);
        if (size == 1)
            printf("  movsx eax, BYTE PTR [rax]\n");
        else if (size == 4)
            printf("  mov eax, [rax]\n");
        else 
            printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    }

    if (node->kind == ND_SIZEOF) {
        printf("  mov rax, %d\n"
               "  push %d\n", node->val, node->val);
        stack_depth += 8;
        return;
    }

    if (node->kind == ND_VARDECL) {
        if (node->rhs == NULL)
            return;

        if (node->rhs->kind == ND_ARRAY) {
            int len = vec_len(node->rhs->block);
            Type *elem_type = node->lhs->type->ptr_to;
            for (int i = 0; i < len; i++) {
                gen_lval(node->lhs);
                printf("  pop rax\n");
                printf("  add rax, %d\n", i * type_size(elem_type));
                printf("  push rax\n");
                Node *e = vec_at(node->rhs->block, i);
                gen(e);
                char *ax = ax_of_type(elem_type);
                printf("  pop rax\n"
                       "  pop rdi\n"
                       "  mov [rdi], %s\n", ax);
            }
            return;
        }

        gen_lval(node->lhs);
        gen(node->rhs);
        printf("  pop rax\n");
        printf("  pop rdi\n");
        char *ax = ax_of_type(node->type);
        printf("  mov [rdi], %s\n", ax);
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

    if (node->kind == ND_GVAR) {
        if (node->type->ty == TY_ARRAY) {
            printf("  mov rax, OFFSET FLAT:");
            fnputs(stdout, node->name, node->len);
            printf("\n");
        } else {
            char *ax = ax_of_type(node->type);
            char *wo = word_of_type(node->type);
            printf("  mov %s, %s PTR ", ax, wo);
            fnputs(stdout, node->name, node->len);
            printf("[rip]\n");
        }

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
    printf("  sub rsp, %d\n", local_vars_space);
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
    } else if (node->kind == ND_GVAR) {
        printf("  mov rax, OFFSET FLAT:");
        fnputs(stdout, node->name, node->len);
        printf("\n");
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
        int coeff = type_size(rt->ptr_to);
        if (coeff != 1)
            printf("  imul rax, %d\n", coeff);
    } else if (rt->ty == TY_INT) {
        int coeff = type_size(lt->ptr_to);
        if (coeff != 1)
            printf("  imul rdi, %d\n", coeff);
    } else {
        error("addition/subtraction of two pointers is not allowed");
    }
}


char *ax_of_type(Type *type) {
    if (type->ty == TY_ARRAY)
        return "rax";
    int size = type_size(type);
    if (size == 1)
        return "al";
    if (size == 4)
        return "eax";
    return "rax";
}

char *di_of_type(Type *type) {
    if (type->ty == TY_ARRAY)
        return "rdi";
    int size = type_size(type);
    if (size == 1)
        return "dil";
    if (size == 4)
        return "edi";
    return "rdi";
}

char *word_of_type(Type* type) {
    if (type->ty == TY_ARRAY)
        return "QWORD";
    return type_size(type) == 8 ? "QWORD" : "DWORD";
}
