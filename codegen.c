#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

char *arg_regs64[6] = {"rdi", "rsi", "rdx", "rcx", "r8",  "r9"};

void gen_globals();
void gen_const(Type*, Node*);
Node *gen_const_calc(Node*);
void gen_stmt(Node*);
void gen_func(Func*);
void gen_lval(Node*);
void gen_coeff_ptr(Type*, Type*);
char *ax_of_type(Type*);
char *word_of_type(Type*);

// generate global variables

void gen_globals() {
    printf("  .data\n");
    for (int i = 0; i < vec_len(global_vars->values); i++) {
        Node *global = vec_at(global_vars->values, i);
        if (global->is_extern)
            continue;

        printf("  .globl %s\n", global->name);
    }

    for (int i = 0; i < vec_len(global_vars->values); i++) {
        Node *global = vec_at(global_vars->values, i);
        if (global->is_extern)
            continue;
        if (is_enum(global->type))
            continue;

        printf("%s:\n", global->name);
        if (global->rhs == NULL) {
            printf("  .zero %d\n", type_size(global->type));
        }
        else {
            Node *rhs = gen_const_calc(global->rhs);
            gen_const(global->type, rhs);
        }
    }
    printf("  .text\n");
    // TODO: Don't need to generate literals used in a char array
    for (int i = 0; i < vec_len(string_literals); i++) {
        char *str = vec_at(string_literals, i);
        printf(".LC%d:\n", i);
        printf("  .string \"%s\"\n", escape_string(str));
    }
}

void gen_const(Type *typ, Node *node) {
    if (node->kind == ND_NUM && node->type == type_int) {
        printf("  .long %d\n", node->val);
        return;
    }
    if (node->kind == ND_ADDR) {
        Node *var = node->lhs;
        printf("  .quad %s\n", var->name);
        return;
    }
    if (node->kind == ND_GVAR) {
        if (is_enum(node->type)) {
            int len = vec_len(node->type->enums);
            for (int i = 0; i < len; i++) {
                Token *e = vec_at(node->type->enums, i);
                if (!strcmp(node->name, e->str)) {
                    printf("  .long %d # enum\n", i);
                    return;
                }
            }
            error_loc(node->loc, "[internal] enum");
        }

        printf("  .quad %s\n", node->name);
        printf("\n");
        return;
    }
    if (node->kind == ND_STRING) {
        if (typ->ty == TY_PTR) {
            int len = vec_len(string_literals);
            for (int i = 0; i < len; i++) {
                char *str = vec_at(string_literals, i);
                if (!strcmp(str, node->name)) {
                    printf("  .quad .LC%d\n", i);
                    return;
                }
            }
        } else if (typ->ty == TY_ARRAY) {
            int len = strlen(node->name);
            printf("  .string \"%s\"\n", escape_string(node->name));
            if (len+1 < typ->array_size)
                printf("  .zero %d\n", (typ->array_size - len));
        } else
            error_loc(node->loc, "[interval] unexpected string occurred");
        return;
    }
    if (node->kind == ND_ARRAY) {
        int arr_len = vec_len(node->block);
        for (int i = 0; i < arr_len; i++)
            gen_const(typ->ptr_to, vec_at(node->block, i));

        if (arr_len < node->type->array_size)
            printf("  .zero %d\n",
                    (node->type->array_size - arr_len) * type_size(node->type->ptr_to));

        return;
    }
    if (node->kind == ND_ADD) {
        Node *var = node->kind == ND_ADDR ? node->lhs->lhs : node->lhs;
        Node *offset = node->rhs;

        printf("  .quad %s + %d\n", var->name, offset->val * type_size(offset->type));
        return;
    }
    if (node->kind == ND_SUB) {
        Node *var = node->lhs->lhs;
        Node *offset = node->rhs;

        printf("  .quad %s - %d\n", var->name, offset->val * type_size(offset->type));
        return;
    }

    error_loc(node->loc, "[codegen] unsupported expression in a global variable declaration");
}

Node *gen_const_calc(Node *node) {
    switch (node->kind) {
    case ND_NUM: case ND_ADDR: case ND_GVAR: case ND_STRING: case ND_ARRAY:
        break;
    case ND_ADD:
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
                Node *new_rhs = mknum(node->lhs->rhs->val + node->rhs->val, node->loc);
                node->lhs = node->lhs->lhs;
                node->rhs = new_rhs;
            } else if (node->lhs->kind == ND_SUB) {
                Node *new_rhs = mknum(-node->lhs->rhs->val + node->rhs->val, node->loc);
                node->lhs = node->lhs->lhs;
                node->rhs = new_rhs;
            }
        }
        break;
    case ND_SUB:
        gen_const_calc(node->lhs);
        gen_const_calc(node->rhs);
        if (is_integer(node->lhs->type) && is_integer(node->rhs->type)) {
            node->type = ND_NUM;
            node->val = node->lhs->val - node->rhs->val;
        } else {
            if (node->lhs->kind == ND_ADD) {
                Node *new_rhs = mknum(node->lhs->rhs->val - node->rhs->val, node->loc);
                node->lhs = node->lhs->lhs;
                node->rhs = new_rhs;
            } else if (node->lhs->kind == ND_SUB) {
                Node *new_rhs = mknum(-node->lhs->rhs->val - node->rhs->val, node->loc);
                node->lhs = node->lhs->lhs;
                node->rhs = new_rhs;
            }
        }
        break;
    default:
        error_loc(node->loc, "[codegen] unsupported expression in a global variable declaration");
    }
    return node;
}

// generate function

int label_num = 0;
int stack_depth = 0;

void gen_expr(Node *node) {
    if (node->kind == ND_NUM) {
        // assuming `int'
        printf("  mov eax, %d\n", node->val);
        printf("  push %d\n", node->val);
        stack_depth += 8;
        return;
    }
    if (node->kind == ND_CHAR) {
        printf("  mov ax, %d\n", (char) node->val);
        printf("  push %d\n", node->val);
        stack_depth += 8;
        return;
    }
    if (node->kind == ND_STRING) {
        for (int i = 0; i < vec_len(string_literals); i++) {
            char *str = vec_at(string_literals, i);
            if (!strcmp(node->name, str)) {
                printf("  mov rax, OFFSET FLAT:.LC%d\n", i);
                printf("  push rax\n");
                stack_depth += 8;
                return;
            }
        }
    }

    if (node->kind == ND_VAR) {
        if (is_enum(node->type)) {
            int len = vec_len(node->type->enums);
            for (int i = 0; i < len; i++) {
                Token *e = vec_at(node->type->enums, i);
                if (!strcmp(node->name, e->str)) {
                    printf("  mov rax, %d\n", i);
                    printf("  push %d\n", i);
                    stack_depth += 8;
                    return;
                }
            }
            error_loc(node->loc, "[internal] enum");
        }

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

    if (node->kind == ND_SEQ) {
        gen_expr(node->lhs);
        printf("  pop rax\n");
        stack_depth -= 8;
        gen_expr(node->rhs);
        return;
    }

    if (node->kind == ND_ASGN) {
        gen_lval(node->lhs);
        gen_expr(node->rhs);
        char *ax = ax_of_type(node->type);
        printf("  pop rax\n"
               "  mov rdi, [rsp]\n");
        printf("  mov [rdi], %s\n", ax);
        printf("  mov [rsp], rax\n");
        stack_depth -= 8;
        return;
    }

    if (node->kind == ND_CALL) {
        int arg_len = vec_len(node->block);
        int revert_stack_depth = stack_depth;
        for (int i = 0; i < arg_len; i++)
            gen_expr(vec_at(node->block, i));
        for (int i = arg_len-1; i >= 0; i--)
            printf("  pop %s\n", arg_regs64[i]);

        stack_depth = revert_stack_depth;
        int diff = (16 - stack_depth % 16) % 16;
        if (diff != 0)
            printf("  sub rsp, %d\n", diff); // 16-bit boundary
        printf("  call %s\n", node->name);
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
        gen_expr(node->lhs);
        int size = type_size(node->type);
        if (size == 1)
            printf("  movsx eax, BYTE PTR [rax]\n");
        else if (size == 4)
            printf("  mov eax, [rax]\n");
        else
            printf("  mov rax, [rax]\n");
        printf("  mov [rsp], rax\n");
        return;
    }

    if (node->kind == ND_ATTR) {
        printf("# prepare attribute access\n");
        gen_lval(node->lhs);
        printf("# offset \n");
        printf("  add rax, %d\n", node->val);
        if (node->type->ty == TY_ARRAY) {
            // pass; put an beginning address of given array
        } else {
            int size = type_size(node->type);
            if (size == 1)
                printf("  movsx eax, BYTE PTR [rax]\n");
            else if (size == 4)
                printf("  mov eax, [rax]\n");
            else
                printf("  mov rax, [rax]\n");
        }
        printf("  mov [rsp], rax\n");
        return;
    }

    if (node->kind == ND_SIZEOF) {
        printf("  mov rax, %d\n"
               "  push %d\n", node->val, node->val);
        stack_depth += 8;
        return;
    }

    if (node->kind == ND_NEG) {
        gen_expr(node->lhs);
        printf("  cmp rax, 0\n"
               "  sete al\n"
               "  movzb eax, al\n"
               "  mov [rsp], rax\n");
        return;
    }

    if (node->kind == ND_BCOMPL) {
        gen_expr(node->lhs);
        printf("  not rax\n"
               "  mov [rsp], rax\n");
        return;
    }

    if (node->kind == ND_GVAR) {
        if (node->type->ty == TY_ARRAY) {
            printf("  mov rax, OFFSET FLAT:%s\n", node->name);
        } else if (is_enum(node->type)) {
            int len = vec_len(node->type->enums);
            for (int i = 0; i < len; i++) {
                Token *e = vec_at(node->type->enums, i);
                if (!strcmp(node->name, e->str)) {
                    printf("  mov rax, %d\n", i);
                    printf("  push %d\n", i);
                    stack_depth += 8;
                    return;
                }
            }
            error_loc(node->loc, "[internal] enum");
        } else {
            char *ax = ax_of_type(node->type);
            char *wo = word_of_type(node->type);
            printf("  mov %s, %s PTR %s[rip]\n", ax, wo, node->name);
        }

        printf("  push rax\n");
        stack_depth += 8;
        return;
    }

    if (node->kind == ND_COND) {
        gen_expr(node->cond); // +8
        printf("  pop rax\n"
               "  cmp rax, 0\n"
               "  je .Lcond_else%d\n", label_num);
        stack_depth -= 8;
        gen_expr(node->lhs); // +8
        stack_depth -= 8;
        printf("  jmp .Lcond_end%d\n"
               ".Lcond_else%d:\n", label_num, label_num);
        gen_expr(node->rhs); // +8
        printf(".Lcond_end%d:\n", label_num);

        label_num++;
        return;
    }

    if (node->kind == ND_LAND) {
        printf("# logical AND operation\n");
        gen_expr(node->lhs);
        printf("  cmp rax, 0\n"
               "  je .Land_end%d\n", label_num);
        printf("  pop rax\n");
        gen_expr(node->rhs);
        printf("  cmp rax, 0\n"
               "  je .Land_end%d\n", label_num);
        printf("  pop rax\n"
               "  push 1\n");
        printf(".Land_end%d:\n", label_num);

        label_num++;
        return;
    }

    if (node->kind == ND_LOR) {
        printf("# logical OR operation\n");
        gen_expr(node->lhs);
        printf("  cmp rax, 0\n"
               "  jne .Lor_true%d\n", label_num);
        gen_expr(node->rhs);
        printf("  cmp rax, 0\n"
               "  je .Lor_end%d\n", label_num);
        printf(".Lor_true%d:\n", label_num);
        printf("  pop rax\n"
               "  push 1\n");
        printf(".Lor_end%d:\n", label_num);

        label_num++;
        return;
    }

    if (node->kind == ND_LSH || node->kind == ND_RSH) {
        printf("# prepare shift\n");
        gen_expr(node->lhs);
        printf("# right hand side\n");
        gen_expr(node->rhs);
        printf("  pop rcx\n");
        printf("  mov rax, [rsp]\n");
        stack_depth -= 8;
        char *mne = node->kind == ND_LSH ? "shl" : "shr";
        char *ax = ax_of_type(node->lhs->type);
        printf("  %s %s, cl\n", mne, ax);
        printf("  mov [rsp], rax\n");
        return;
    }

    if (node->kind == ND_PREINCR || node->kind == ND_PREDECR) {
        int sign = node->kind == ND_PREINCR ? 1 : -1;
        int incr = sign * (is_pointer(node->type) ? type_size(node->type->ptr_to) : 1);
        gen_lval(node->lhs);
        printf("  mov rdi, [rax]\n"
               "  add rdi, %d\n", incr);
        printf("  mov [rax], rdi\n"
               "  mov rax, rdi\n"
               "  mov [rsp], rdi\n");
        return;
    }

    if (node->kind == ND_POSTINCR || node->kind == ND_POSTDECR) {
        int sign = node->kind == ND_POSTINCR ? 1 : -1;
        int incr = sign * (is_pointer(node->type) ? type_size(node->type->ptr_to) : 1);
        gen_lval(node->lhs);
        printf("  mov rdi, [rax]\n"
               "  mov [rsp], rdi\n"
               "  add rdi, %d\n", incr);
        printf("  mov [rax], rdi\n"
               "  mov rax, [rsp]\n");
        return;
    }

    if (ND_ADDEQ <= node->kind && node->kind <= ND_XOREQ) {
        printf("# prepare assignment operation\n");
        gen_lval(node->lhs);
        printf("# prepare assignment operation; right hand side\n");
        gen_expr(node->rhs);

        switch (node->kind) {
            case ND_LSHEQ: case ND_RSHEQ: {
                printf("  pop rcx\n"
                       "  mov rdi, [rsp]\n"
                       "  mov %s, [rdi]\n", ax_of_type(node->lhs->type));
                stack_depth -= 8;
                char *ax = ax_of_type(node->lhs->type);
                switch (node->kind) {
                    case ND_LSHEQ:
                        printf("  shl %s, cl\n", ax);
                        break;
                    case ND_RSHEQ:
                        printf("  shr %s, cl\n", ax);
                        break;
                    default:
                        error("[internal] unreachable");
                }
                printf("  mov [rdi], rax\n"
                       "  mov [rsp], rax\n");
                break;
            }
            default:
                printf("  pop rdi\n"
                       "  mov rax, [rsp]\n"
                       "  mov %s, [rax]\n", ax_of_type(node->lhs->type));
                stack_depth -= 8;
                switch (node->kind) {
                    case ND_ADDEQ:
                        gen_coeff_ptr(node->lhs->type, node->rhs->type);
                        printf("  add rax, rdi\n");
                        break;
                    case ND_SUBEQ:
                        gen_coeff_ptr(node->lhs->type, node->rhs->type);
                        printf("  sub rax, rdi\n");
                        break;
                    case ND_MULEQ:
                        printf("  imul rax, rdi\n");
                        break;
                    case ND_DIVEQ:
                        printf("  cqo\n"
                               "  idiv rdi\n");
                        break;
                    case ND_MODEQ:
                        printf("  cqo\n"
                               "  idiv rdi\n"
                               "  mov rax, rdx\n");
                        break;
                    case ND_IOREQ:
                        printf("  or rax, rdi\n");
                        break;
                    case ND_XOREQ:
                        printf("  xor rax, rdi\n");
                        break;
                    case ND_ANDEQ:
                        printf("  and rax, rdi\n");
                        break;
                    default:
                        error("[internal] unreachable\n");
                }
                printf("  mov rdi, [rsp]\n"
                       "  mov [rdi], rax\n"
                       "  mov [rsp], rax\n");
        }
        return;
    }

    printf("# prepare arithmetic operation\n");
    gen_expr(node->lhs);
    printf("# right hand side\n");
    gen_expr(node->rhs);

    printf("# arithmetic operation\n");
    printf("  pop rdi\n");
    printf("  mov rax, [rsp]\n");
    stack_depth -= 8;

    switch (node->kind) {
        case ND_ADD:
            gen_coeff_ptr(node->lhs->type, node->rhs->type);
            printf("  add rax, rdi\n");
            break;
        case ND_SUB:
            gen_coeff_ptr(node->lhs->type, node->rhs->type);
            printf("  sub rax, rdi\n");
            if (is_pointer_compat(node->lhs->type) && is_pointer_compat(node->rhs->type)) {
                printf("  mov rdi, %d\n", type_size(node->lhs->type->ptr_to));
                printf("  div rdi\n");
            }
            break;
        case ND_MUL:
            printf("  imul rax, rdi\n");
            break;
        case ND_DIV:
            printf("  cqo\n"
                   "  idiv rdi\n");
            break;
        case ND_MOD:
            printf("  cqo\n"
                   "  idiv rdi\n"
                   "  mov rax, rdx\n");
            break;
        case ND_IOR:
            printf("  or rax, rdi\n");
            break;
        case ND_XOR:
            printf("  xor rax, rdi\n");
            break;
        case ND_AND:
            printf("  and rax, rdi\n");
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
    printf("  mov [rsp], rax\n");
    printf("# end arithmetic operation\n");
}

void gen_stmt(Node *node) {
    if (node->kind == ND_VARDECL) {
        if (node->rhs == NULL)
            return;

        if (node->rhs->kind == ND_ARRAY) {
            // TODO: Handle nested arrays
            int len = vec_len(node->rhs->block);
            Type *elem_type = node->lhs->type->ptr_to;
            for (int i = 0; i < len; i++) {
                printf("# local array initialization %d", i);
                gen_lval(node->lhs);
                printf("  add rax, %d\n", i * type_size(elem_type));
                printf("  mov [rsp], rax\n");

                printf("# local array initialization %d; right hand side\n", i);
                Node *e = vec_at(node->rhs->block, i);
                gen_expr(e);
                printf("# local array initialization %d; assignment\n", i);
                char *ax = ax_of_type(elem_type);
                printf("  pop rax\n"
                       "  pop rdi\n"
                       "  mov [rdi], %s\n", ax);
                stack_depth -= 16;
            }
            return;
        }

        printf("# variable declaration\n");
        gen_lval(node->lhs);
        printf("# variable declaration; right hand side\n");
        gen_expr(node->rhs);
        printf("# variable declaration; assignment\n");
        printf("  pop rax\n");
        printf("  pop rdi\n");
        char *ax = ax_of_type(node->type);
        printf("  mov [rdi], %s\n", ax);
        stack_depth -= 16;
        return;
    }

    if (node->kind == ND_RETURN) {
        if (node->lhs != NULL) {
            gen_expr(node->lhs);
            printf("  pop rax\n");
        }
        printf("  mov rsp, rbp\n"
               "  pop rbp\n"
               "  ret\n");
        return;
    }

    if (node->kind == ND_IF) {
        printf("# if statement\n");
        gen_expr(node->cond);
        printf("  pop rax\n"
               "  cmp rax, 0\n");
        stack_depth -= 8;
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
        char *label_base = node->name;
        printf(".L%s_cont:\n", label_base);
        gen_expr(node->cond);
        printf("# while jump\n");
        printf("  pop rax\n"
               "  cmp rax, 0\n");
        printf("  je .L%s_end\n", label_base);
        printf("# while body\n");
        stack_depth -= 8;
        gen_stmt(node->body);
        printf("  jmp .L%s_cont\n", label_base);
        printf(".L%s_end:\n", label_base);
        return;
    }

    if (node->kind == ND_FOR) {
        if (node->lhs != NULL)
            gen_stmt(node->lhs);
        printf(".L%s:\n", node->name);
        if (node->cond != NULL) {
            gen_expr(node->cond);
            printf("  pop rax\n"
                   "  cmp rax, 0\n");
            printf("  je .L%s_end\n", node->name);
            stack_depth -= 8;
        }
        gen_stmt(node->body);
        printf(".L%s_cont:\n", node->name);
        if (node->rhs != NULL)
            gen_stmt(node->rhs);
        printf("  jmp .L%s\n", node->name);
        printf(".L%s_end:\n", node->name);
        return;
    }

    if (node->kind == ND_DOWHILE) {
        printf(".L%s:\n", node->name);
        gen_stmt(node->body);
        printf("# prepare do-while repeat check\n");
        printf(".L%s_cont:\n", node->name);
        gen_expr(node->cond);
        printf("# do-while repeat check\n");
        printf("  pop rax\n"
               "  cmp rax, 0\n");
        printf("  jne .L%s\n", node->name);
        printf(".L%s_end:\n", node->name);

        stack_depth -= 8;
        label_num += 1;
        return;
    }

    if (node->kind == ND_CONTINUE) {
        printf("  jmp .L%s_cont\n", node->name);
        return;
    }

    if (node->kind == ND_BREAK) {
        printf("  jmp .L%s_end\n", node->name);
        return;
    }

    if (node->kind == ND_BLOCK) {
        int len = vec_len(node->block);
        for (int i = 0; i < len; i++)
            gen_stmt(vec_at(node->block, i));
        return;
    }

    if (node->kind == ND_SWITCH) {
        gen_expr(node->cond);
        printf("  pop rax\n");
        int len = vec_len(node->block);
        for (int i = 0; i < len; i++) {
            Node *stmt = vec_at(node->block, i);
            if (stmt->kind != ND_CASE)
                continue;
            printf("  cmp rax, %d\n", stmt->lhs->val);
            printf("  je .L%s\n", stmt->name);
        }
        bool defaulted = false;
        for (int i = 0; i < len; i++) {
            Node *stmt = vec_at(node->block, i);
            if (stmt->kind == ND_DEFAULT) {
                defaulted = true;
                printf("  jmp .L%s\n", stmt->name);
                break;
            }
        }
        if (!defaulted)
            printf("  jmp .L%s_end\n", node->name);
        for (int i = 0; i < len; i++)
            gen_stmt(vec_at(node->block, i));
        printf(".L%s_end:\n", node->name);
        return;
    }

    if (node->kind == ND_CASE || node->kind == ND_DEFAULT) {
        printf(".L%s:\n", node->name);
        return;
    }

    gen_expr(node);
    printf("  pop rax # throw away from stack\n");
    stack_depth -= 8;
}

void gen_func(Func* func) {
    if (func->is_extern)
        return;

    printf("%s:\n", func->name);
    printf("  push rbp\n"
           "  mov rbp, rsp\n");
    for (int i = 0; i < vec_len(func->params); i++)
        printf("  push %s\n", arg_regs64[i]);
    int local_vars_space = func->offset - 8 * vec_len(func->params);
    printf("  sub rsp, %d\n", local_vars_space);
    stack_depth = func->offset;
    for (int i = 0; i < vec_len(func->block); i++) {
        printf("# %s statement %d\n", func->name, i);
        gen_stmt(vec_at(func->block, i));
    }
    printf("# return %s\n", func->name);
    printf("  mov rsp, rbp\n"
           "  pop rbp\n"
           "  ret\n");
}

void gen_lval(Node *node) {
    if (node->kind == ND_VAR) {
        printf("# load local variable %s\n", node->name);
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->val);
        printf("  push rax\n");
        printf("# end load local variable %s\n", node->name);
        stack_depth += 8;
        return;
    } else if (node->kind == ND_GVAR) {
        printf("# load global variable %s\n", node->name);
        printf("  mov rax, OFFSET FLAT:%s\n", node->name);
        printf("  push rax\n");
        printf("# end load global variable %s\n", node->name);
        stack_depth += 8;
        return;
    } else if (node->kind == ND_DEREF) {
        printf("# dereferencing\n");
        gen_expr(node->lhs);
        printf("# end dereferencing\n");
        return;
    } else if (node->kind == ND_NUM) {
        gen_expr(node);
        return;
    } else if (node->kind == ND_ATTR) {
        printf("# prepare attribute access\n");
        gen_lval(node->lhs);
        printf("  pop rax\n"
               "  add rax, %d\n", node->val);
        printf("  push rax\n");
        return;
    }
    error("term should be a left value");
}

void gen_coeff_ptr(Type* lt /* rax */, Type* rt /* rdi */) {
    if (is_pointer_compat(lt) && is_pointer_compat(rt)) {
    } else if (lt->ty == TY_INT && rt->ty == TY_INT) {
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

char *word_of_type(Type* type) {
    if (type->ty == TY_ARRAY)
        return "QWORD";
    return type_size(type) == 8 ? "QWORD" : "DWORD";
}
