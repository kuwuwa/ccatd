#include "ccatd.h"

char *arg_regs64[6] = {"rdi", "rsi", "rdx", "rcx", "r8",  "r9"};

void gen_globals();
static void gen_const(Type* t, Node* n);
static Node *gen_const_calc(Node* n);
void gen_func(Func* f);
static void gen_stmt(Node* n, Func *f);
static void gen_lval(Node* n, Func *f);
static void gen_coeff_ptr(Type* t1, Type* t2);
static void extend_rax(int dst, int src);
static void load_rax(int size);
static char *rax_of_type(Type* t);
static char *rdi_of_type(Type* t);

static int resolve_enum_value(Vec *enums, char *name) {
    int len = vec_len(enums);
    for (int i = 0; i < len; i++) {
        if (!strcmp(name, ((Token*) vec_at(enums, i))->str))
            return i;
    }
    error("[internal] enum");
    return -1;
}

// generate global variables

void gen_globals() {
    printf("  .data\n");
    for (int i = 0; i < vec_len(global_vars->values); i++) {
        Node *global = vec_at(global_vars->values, i);
        if (global->is_extern || global->is_static)
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
    for (int i = 0; i < vec_len(string_literals); i++) {
        char *str = vec_at(string_literals, i);
        printf(".LC%d:\n", i);
        printf("  .string \"%s\"\n", escape_string(str));
    }
}

void gen_const(Type *typ, Node *node) {
    switch (node->kind) {
    case ND_NUM:
        printf("  .long %d\n", node->val);
        return;
    case ND_ADDR:
        printf("  .quad %s\n", node->lhs->name);
        return;
    case ND_GVAR:
        if (is_enum(node->type)) {
            int len = vec_len(node->type->enums);
            for (int i = 0; i < len; i++) {
                Token *e = vec_at(node->type->enums, i);
                if (!strcmp(node->name, e->str)) {
                    printf("  .long %d\n", i);
                    return;
                }
            }
            error_loc(node->loc, "[internal] enum");
        }

        printf("  .quad %s\n", node->name);
        printf("\n");
        return;
    case ND_STRING:
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
    case ND_ARRAY: {
        int arr_len = vec_len(node->block);
        for (int i = 0; i < arr_len; i++)
            gen_const(typ->ptr_to, vec_at(node->block, i));

        int blank = node->type->array_size - arr_len;
        if (blank > 0)
            printf("  .zero %d\n", blank * type_size(node->type->ptr_to));

        return;
    }
    case ND_ADD: {
        Node *var = (node->lhs->type->ty == TY_ARRAY) ? node->lhs
                  : (node->lhs->kind == ND_ADDR) ? node->lhs->lhs
                  : (error_loc(node->loc, "[codegen] not constant"), NULL);
        int size = type_size(node->lhs->type->ptr_to);
        Node *offset = node->rhs;
        printf("  .quad %s + %d\n", var->name, offset->val * size);
        return;
    }
    case ND_SUB: {
        Node *var = (node->lhs->type->ty == TY_ARRAY) ? node->lhs
                  : (node->lhs->kind == ND_ADDR) ? node->lhs->lhs
                  : (error_loc(node->loc, "[codegen] not constant"), NULL);
        int size = type_size(node->lhs->type->ptr_to);
        Node *offset = node->rhs;
        printf("  .quad %s - %d\n", var->name, offset->val * size);
        return;
    }
    default:
        error_loc(node->loc, "[codegen] unsupported expression in a global variable declaration");
    }
}

int add_or_sub(Node_kind kind, int a, int b) {
    if (kind == ND_ADD)
        return a + b;
    return a - b;
}

Node *gen_const_calc(Node *node) {
    switch (node->kind) {
    case ND_NUM: case ND_ADDR: case ND_GVAR: case ND_STRING: case ND_ARRAY:
        break;
    case ND_ADD: case ND_SUB:
        gen_const_calc(node->lhs);
        gen_const_calc(node->rhs);
        if (is_integer(node->lhs->type) && is_integer(node->rhs->type)) {
            node->type = binary_int_op_result(node->lhs->type, node->rhs->type);
            node->val = add_or_sub(node->kind, node->lhs->val, node->rhs->val);
        } else {
            if (node->kind == ND_ADD && is_pointer_compat(node->rhs->type)) {
                Node *tmp = node->rhs;
                node->rhs = node->lhs;
                node->lhs = tmp;
            }

            if (node->lhs->kind == ND_ADD) {
                Node *new_rhs = mknum(
                        add_or_sub(node->kind, node->lhs->rhs->val, node->rhs->val),
                        node->loc);
                node->lhs = node->lhs->lhs;
                node->rhs = new_rhs;
            } else if (node->lhs->kind == ND_SUB) {
                Node *new_rhs = mknum(
                        add_or_sub(node->kind, -node->lhs->rhs->val, node->rhs->val),
                        node->loc);
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

bool is_assign_expr(Node_kind kind) {
    switch (kind) {
        case ND_ADDEQ: case ND_SUBEQ: case ND_MULEQ:
        case ND_DIVEQ: case ND_MODEQ: case ND_ANDEQ:
        case ND_IOREQ: case ND_XOREQ: case ND_LSHEQ:
        case ND_RSHEQ:
            return true;
        default:
            return false;
    }
}

int label_num = 0;
int stack_depth = 0;

void gen_expr(Node *node, Func *func) {
    switch (node->kind) {
    case ND_NUM:
        // assuming `int'
        printf("  mov eax, %d\n", node->val);
        printf("  push %d\n", node->val);
        return;
    case ND_CHAR:
        printf("  mov al, %d\n", node->val);
        printf("  push %d\n", node->val);
        return;
    case ND_STRING:
        for (int i = 0; i < vec_len(string_literals); i++) {
            char *str = vec_at(string_literals, i);
            if (!strcmp(node->name, str)) {
                printf("  mov rax, OFFSET .LC%d\n", i);
                printf("  push rax\n");
                return;
            }
        }
        error_loc(node->loc, "[internal] string not found\n");
    case ND_VAR:
        if (node->is_enum) {
            int idx = resolve_enum_value(node->type->enums, node->name);
            printf("  mov rax, %d\n", idx);
            printf("  push %d\n", idx);
            return;
        }

        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->val);

        if (node->type->ty != TY_ARRAY)
            load_rax(type_size(node->type));
        printf("  push rax\n");
        return;
    case ND_GVAR:
        if (node->type->ty == TY_ARRAY)
            printf("  mov rax, OFFSET %s\n", node->name);
        else if (is_enum(node->type)) {
            int idx = resolve_enum_value(node->type->enums, node->name);
            printf("  mov rax, %d\n", idx);
            printf("  push %d\n", idx);
            return;
        } else {
            char *rax = rax_of_type(node->type);
            printf("  mov rax, OFFSET %s\n", node->name);
            printf("  mov %s, [rax]\n", rax);
        }

        printf("  push rax\n");
        return;
    case ND_SEQ:
        gen_expr(node->lhs, func);
        printf("  pop rax\n");
        gen_expr(node->rhs, func);
        return;
    case ND_ASGN:
        gen_lval(node->lhs, func);
        stack_depth += 8;
        gen_expr(node->rhs, func);
        stack_depth -= 8;
        printf("  pop rax\n");
        extend_rax(type_size(node->lhs->type), type_size(node->rhs->type));
        printf("  mov rdi, [rsp]\n");

        char *rax = rax_of_type(node->type);
        printf("  mov [rdi], %s\n", rax);
        printf("  mov [rsp], %s\n", rax);
        return;
    case ND_CALL: {
        if (!strcmp("__builtin_va_start", node->name)) {
            // assuming this call is in va_start called by a variadic function
            printf("  mov rax, [rbp-56]\n"); // ap
            printf("  mov rdi, [rbp]\n");
            printf("  add rdi, QWORD PTR [rdi-8]\n");
            printf("  sub rdi, 56\n");
            printf("  mov DWORD PTR [rax], 48\n"); // gp_offset
            printf("  mov DWORD PTR [rax+4], 304\n"); // fp_offset
            printf("  mov QWORD PTR [rax+8], rdi\n"); // overflow_arg_area
            printf("  mov QWORD PTR [rax+16], 0\n"); // reg_save_area
            printf("  mov rax, 0\n"); // # of floating point parameters
            printf("  push rax\n");
            return;
        }

        int arg_len = vec_len(node->block);
        Func *called = map_find(func_env, node->name);
        for (int i = 0; i < arg_len; i++) {
            Node *e = vec_at(node->block, i);
            gen_expr(e, func);

            if (i >= vec_len(called->params)) // varargs
                continue;
            int size_arg = type_size(coerce_pointer(e->type));
            Node *p = vec_at(called->params, i);
            int size_param = type_size(p->type);
            if (size_arg < size_param && type_size(coerce_pointer(e->type)) == 1) {
                char *rax = rax_of_type(p->type);
                printf("  movzb eax, al\n");
                printf("  mov [rsp], %s\n", rax);
            }
        }

        for (int i = arg_len-1; i >= 0; i--)
            printf("  pop %s\n", arg_regs64[i]);

        int diff = (16 - stack_depth % 16) % 16;
        if (diff != 0)
            printf("  sub rsp, %d\n", diff); // 16-bit boundary
        printf("  call %s\n", node->name);
        if (diff != 0)
            printf("  add rsp, %d\n", diff); // 16-bit boundary

        printf("  push rax\n");
        return;
    }
    case ND_ADDR:
        gen_lval(node->lhs, func);
        return;
    case ND_DEREF:
        gen_expr(node->lhs, func);
        load_rax(type_size(node->type));
        printf("  mov [rsp], %s\n", rax_of_type(node->type));
        return;
    case ND_ATTR:
        gen_lval(node->lhs, func);
        printf("  add rax, %d\n", node->val);
        if (node->type->ty != TY_ARRAY)
            load_rax(type_size(node->type));
        printf("  mov [rsp], %s\n", rax_of_type(node->type));
        return;
    case ND_CAST:
        gen_expr(node->lhs, func);
        node->lhs->type = node->type;
        return;
    case ND_SIZEOF:
        printf("  mov rax, %d\n"
               "  push %d\n", node->val, node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs, func);
        printf("  cmp %s, 0\n", rax_of_type(node->type));
        printf("  sete al\n"
               "  movzb eax, al\n"
               "  mov [rsp], rax\n");
        return;
    case ND_BCOMPL:
        gen_expr(node->lhs, func);
        printf("  not %s\n", rax_of_type(node->type));
        printf("  mov [rsp], %s\n", rax_of_type(node->type));
        return;
    case ND_COND: {
        int lb = label_num++;
        gen_expr(node->cond, func);
        char *rax = rax_of_type(node->cond->type);
        printf("  pop rax\n"
               "  cmp %s, 0\n", rax);
        printf("  je .Lcond_else%d\n", lb);
        gen_expr(node->lhs, func);
        printf("  jmp .Lcond_end%d\n"
               ".Lcond_else%d:\n", lb, lb);
        gen_expr(node->rhs, func);
        printf(".Lcond_end%d:\n", lb);
        return;
    }
    case ND_LAND: {
        int lb = label_num++;
        gen_expr(node->lhs, func);
        char *raxl = rax_of_type(node->lhs->type);
        printf("  cmp %s, 0\n", raxl);
        printf("  je .Land_end%d\n", lb);
        printf("  pop rax\n");
        char *raxr = rax_of_type(node->rhs->type);
        gen_expr(node->rhs, func);
        printf("  cmp %s, 0\n", raxr);
        printf("  je .Land_end%d\n", lb);
        printf("  pop rax\n"
               "  push 1\n");
        printf(".Land_end%d:\n", lb);
        return;
    }
    case ND_LOR: {
        int lb = label_num++;
        gen_expr(node->lhs, func);
        char *raxl = rax_of_type(node->lhs->type);
        printf("  cmp %s, 0\n", raxl);
        printf("  jne .Lor_true%d\n", lb);
        printf("  pop rax\n");
        gen_expr(node->rhs, func);
        char *raxr = rax_of_type(node->rhs->type);
        printf("  cmp %s, 0\n", raxr);
        printf("  je .Lor_end%d\n", lb);
        printf(".Lor_true%d:\n", lb);
        printf("  pop rax\n"
               "  push 1\n");
        printf(".Lor_end%d:\n", lb);
        return;
    }
    case ND_PREINCR: case ND_PREDECR: {
        int sign = node->kind == ND_PREINCR ? 1 : -1;
        int incr = sign * (is_pointer(node->type) ? type_size(node->type->ptr_to) : 1);
        gen_lval(node->lhs, func);

        char *rdi = rdi_of_type(node->lhs->type);
        char *rax = rax_of_type(node->lhs->type);
        printf("  mov %s, [rax]\n", rdi);
        printf("  add %s, %d\n", rdi, incr);
        printf("  mov [rax], %s\n", rdi);
        printf("  mov %s, %s\n", rax, rdi);
        printf("  mov [rsp], %s\n", rdi);
        return;
    }
    case ND_POSTINCR: case ND_POSTDECR: {
        int sign = node->kind == ND_POSTINCR ? 1 : -1;
        int incr = sign * (is_pointer(node->type) ? type_size(node->type->ptr_to) : 1);
        gen_lval(node->lhs, func);

        char *rdi = rdi_of_type(node->lhs->type);
        char *rax = rax_of_type(node->lhs->type);
        printf("  mov %s, [rax]\n", rdi);
        printf("  mov [rsp], %s\n", rdi);
        printf("  add %s, %d\n", rdi, incr);
        printf("  mov [rax], %s\n", rdi);
        printf("  mov %s, [rsp]\n", rax);
        return;
    }
    case ND_LSHEQ: case ND_LSH:
    case ND_RSHEQ: case ND_RSH: {
        bool assign = is_assign_expr(node->kind);

        assign ? gen_lval(node->lhs, func) : gen_expr(node->lhs, func);
        stack_depth += 8;
        gen_expr(node->rhs, func);
        stack_depth -= 8;
        printf("  pop rcx\n");

        char *rax = rax_of_type(node->lhs->type);
        if (assign) {
            printf("  mov rdi, [rsp]\n");
            printf("  mov %s, [rdi]\n", rax);
        } else {
            printf("  mov %s, [rsp]\n", rax);
        }

        char *mne = (node->kind == ND_LSH || node->kind == ND_LSHEQ) ? "shl" : "shr";
        printf("  %s %s, cl\n", mne, rax);

        if (assign)
            printf("  mov [rdi], %s\n", rax);
        printf("  mov [rsp], %s\n", rax);
        return;
    }
    default:
        break;
    } // switch end

    bool assign = is_assign_expr(node->kind);

    assign ? gen_lval(node->lhs, func) : gen_expr(node->lhs, func);
    gen_expr(node->rhs, func);

    // rhs
    printf("  pop rax\n");
    extend_rax(type_size(node->type), type_size(coerce_pointer(node->rhs->type)));
    printf("  mov rdi, rax\n");

    char *rax = rax_of_type(node->type);
    char *rdi = rdi_of_type(node->type);

    // lhs
    printf("  mov rax, [rsp]\n");
    if (assign)
        printf("  mov %s, [rax]\n", rax);
    extend_rax(type_size(node->type), type_size(coerce_pointer(node->lhs->type)));

    switch (node->kind) {
        case ND_ADD: case ND_ADDEQ:
            gen_coeff_ptr(node->lhs->type, node->rhs->type);
            printf("  add %s, %s\n", rax, rdi);
            break;
        case ND_SUB: case ND_SUBEQ:
            gen_coeff_ptr(node->lhs->type, node->rhs->type);
            printf("  sub %s, %s\n", rax, rdi);
            if (is_pointer_compat(node->lhs->type) && is_pointer_compat(node->rhs->type)) {
                printf("  mov rdi, %d\n", type_size(node->lhs->type->ptr_to));
                printf("  cqo\n"
                       "  div %s\n", rdi);
            }
            break;
        case ND_MUL: case ND_MULEQ:
            printf("  imul %s, %s\n", rax, rdi);
            break;
        case ND_DIV: case ND_DIVEQ:
            printf("  cqo\n"
                   "  idiv %s\n", rdi);
            break;
        case ND_MOD: case ND_MODEQ: {
            char *mne = type_size(node->type) == 4 ? "cdq"
                     : "cqo";
            printf("  %s\n", mne);
            printf("  idiv %s\n", rdi);
            printf("  mov rax, rdx\n");
            break;
        }
        case ND_IOR: case ND_IOREQ:
            printf("  or %s, %s\n", rax, rdi);
            break;
        case ND_XOR: case ND_XOREQ:
            printf("  xor %s, %s\n", rax, rdi);
            break;
        case ND_AND: case ND_ANDEQ:
            printf("  and %s, %s\n", rax, rdi);
            break;
        default: {
            printf("  cmp %s, %s\n", rax, rdi);
            char *mne = node->kind == ND_EQ ? "sete"
                      : node->kind == ND_NEQ ? "setne"
                      : node->kind == ND_LT ? "setl"
                      : node->kind == ND_LTE ? "setle"
                      : (error("should be unreachable"), NULL);
            printf("  %s al\n", mne);
            printf("  movzb rax, al\n");
        }
    }

    if (assign)
        printf("  mov rdi, [rsp]\n"
               "  mov [rdi], %s\n", rax);
    printf("  mov [rsp], %s\n", rax);

}

void gen_stmt(Node *node, Func *func) {
    switch (node->kind) {
    case ND_VARDECL:
        if (node->rhs == NULL)
            return;

        if (node->rhs->kind == ND_ARRAY) {
            // TODO: Handle nested arrays
            int len = vec_len(node->rhs->block);
            Type *elem_type = node->lhs->type->ptr_to;
            for (int i = 0; i < len; i++) {
                gen_lval(node->lhs, func);
                char *rax = rax_of_type(elem_type);
                printf("  add rax, %d\n", i * type_size(elem_type));
                printf("  mov [rsp], %s\n", rax);

                Node *e = vec_at(node->rhs->block, i);
                stack_depth += 8;
                gen_expr(e, func);
                stack_depth -= 8;
                printf("  pop rax\n"
                       "  pop rdi\n"
                       "  mov [rdi], %s\n", rax);
            }
            return;
        }

        gen_lval(node->lhs, func);
        stack_depth += 8;
        gen_expr(node->rhs, func);
        stack_depth -= 8;
        printf("  pop rax\n");
        printf("  pop rdi\n");
        char *rax = rax_of_type(node->type);
        printf("  mov [rdi], %s\n", rax);
        return;
    case ND_RETURN:
        if (node->lhs != NULL)
            gen_expr(node->lhs, func);
        printf("  jmp .L%s_return\n", func->name);
        return;
    case ND_IF: {
        int lb = label_num++;
        gen_expr(node->cond, func);
        char *rax = rax_of_type(node->cond->type);
        printf("  pop rax\n"
               "  cmp %s, 0\n", rax);
        if (node->rhs == NULL) {
            printf("  je .Lend_if%d\n", lb);
            gen_stmt(node->lhs, func);
            printf(".Lend_if%d:\n", lb);
        } else {
            printf("  je  .Lelse%d\n", lb);
            gen_stmt(node->lhs, func);
            printf("  jmp .Lend_if%d\n", lb);
            printf(".Lelse%d:\n", lb);
            gen_stmt(node->rhs, func);
            printf(".Lend_if%d:\n", lb);
        }
        return;
    }
    case ND_WHILE: {
        char *label_base = node->name;
        printf(".L%s_cont:\n", label_base);
        gen_expr(node->cond, func);
        char *rax = rax_of_type(node->cond->type);
        printf("  pop rax\n"
               "  cmp %s, 0\n", rax);
        printf("  je .L%s_end\n", label_base);
        gen_stmt(node->body, func);
        printf("  jmp .L%s_cont\n", label_base);
        printf(".L%s_end:\n", label_base);
        return;
    }
    case ND_FOR:
        if (node->lhs != NULL)
            gen_stmt(node->lhs, func);
        printf(".L%s:\n", node->name);
        if (node->cond != NULL) {
            gen_expr(node->cond, func);
            char *rax = rax_of_type(node->cond->type);
            printf("  pop rax\n"
                   "  cmp %s, 0\n", rax);
            printf("  je .L%s_end\n", node->name);
        }
        gen_stmt(node->body, func);
        printf(".L%s_cont:\n", node->name);
        if (node->rhs != NULL)
            gen_stmt(node->rhs, func);
        printf("  jmp .L%s\n", node->name);
        printf(".L%s_end:\n", node->name);
        return;
    case ND_DOWHILE:
        printf(".L%s:\n", node->name);
        gen_stmt(node->body, func);
        printf(".L%s_cont:\n", node->name);
        gen_expr(node->cond, func);
        printf("  pop rax\n"
               "  cmp %s, 0\n", rax_of_type(node->cond->type));
        printf("  jne .L%s\n", node->name);
        printf(".L%s_end:\n", node->name);

        label_num++;
        return;
    case ND_CONTINUE:
        printf("  jmp .L%s_cont\n", node->name);
        return;
    case ND_BREAK:
        printf("  jmp .L%s_end\n", node->name);
        return;
    case ND_BLOCK: {
        int len = vec_len(node->block);
        for (int i = 0; i < len; i++)
            gen_stmt(vec_at(node->block, i), func);
        return;
    }
    case ND_SWITCH: {
        gen_expr(node->cond, func);
        printf("  pop rax\n");
        int len = vec_len(node->block);
        for (int i = 0; i < len; i++) {
            Node *stmt = vec_at(node->block, i);
            if (stmt->kind != ND_CASE)
                continue;
            printf("  cmp rax, %d\n", stmt->lhs->val);
            printf("  je .L%s\n", stmt->name);
        }
        bool has_default = false;
        for (int i = 0; i < len; i++) {
            Node *stmt = vec_at(node->block, i);
            if (stmt->kind == ND_DEFAULT) {
                has_default = true;
                printf("  jmp .L%s\n", stmt->name);
                break;
            }
        }
        if (!has_default)
            printf("  jmp .L%s_end\n", node->name);
        for (int i = 0; i < len; i++)
            gen_stmt(vec_at(node->block, i), func);
        printf(".L%s_end:\n", node->name);
        return;
    }
    case ND_CASE: case ND_DEFAULT:
        printf(".L%s:\n", node->name);
        return;
    default:
        gen_expr(node, func);
        printf("  pop rax\n");
    }
}

void gen_func(Func* func) {
    stack_depth = 0;
    if (func->is_extern)
        return;

    printf("%s:\n", func->name);
    printf("  push rbp\n"
           "  mov rbp, rsp\n");

    int params_len = vec_len(func->params);

    if (func->is_varargs) {
        printf("  mov QWORD PTR [rbp-8], %d\n", 8 * params_len);
        printf("  mov [rbp-16], r9\n");
        printf("  mov [rbp-24], r8\n");
        printf("  mov [rbp-32], rcx\n");
        printf("  mov [rbp-40], rdx\n");
        printf("  mov [rbp-48], rsi\n");
        printf("  mov [rbp-56], rdi\n");
        printf("  sub rsp, 56\n");
    }

    for (int i = 0; i < params_len; i++)
        printf("  push %s\n", arg_regs64[i]);

    int local_vars_space = func->offset - 8 * vec_len(func->params);
    local_vars_space -= func->is_varargs ? 56 : 0;
    printf("  sub rsp, %d\n", local_vars_space);
    for (int i = 0; i < vec_len(func->block); i++) {
        stack_depth = func->offset;
        gen_stmt(vec_at(func->block, i), func);
    }
    printf(".L%s_return:\n", func->name);
    printf("mov rsp, rbp\n"
           "  pop rbp\n"
           "  ret\n");
}

void gen_lval(Node *node, Func *func) {
    switch(node->kind) {
    case ND_VAR:
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->val);
        printf("  push rax\n");
        return;
    case ND_GVAR:
        printf("  mov rax, OFFSET %s\n", node->name);
        printf("  push rax\n");
        return;
    case ND_DEREF:
        gen_expr(node->lhs, func);
        return;
    case ND_NUM:
        gen_expr(node, func);
        return;
    case ND_ATTR:
        gen_lval(node->lhs, func);
        printf("  pop rax\n"
               "  add rax, %d\n", node->val);
        printf("  push rax\n");
        return;
    default:
        error("term should be a left value");
    }
}

static void gen_coeff_ptr(Type* lt /* rax */, Type* rt /* rdi */) {
    if (is_pointer_compat(lt) && is_pointer_compat(rt)) {
    } else if (lt->ty == TY_INT && rt->ty == TY_INT) {
    } else if (lt->ty == TY_INT) {
        char *rax = rax_of_type(lt);
        int coeff = type_size(rt->ptr_to);
        if (coeff != 1)
            printf("  imul %s, %d\n", rax, coeff);
    } else if (rt->ty == TY_INT) {
        char *rdi = rdi_of_type(rt);
        int coeff = type_size(lt->ptr_to);
        if (coeff != 1)
            printf("  imul %s, %d\n", rdi, coeff);
    } else
        error("addition/subtraction of two pointers is not allowed");
}

void extend_rax(int dst, int src) {
    if (dst <= src) return;
    if (dst == 4)
        printf("  cbw\n  cwde\n");
    if (dst == 8)
        printf("  cdqe\n");
}

static void load_rax(int size) {
    if (size == 1)
        printf("  movsx eax, BYTE PTR [rax]\n");
    else if (size == 4)
        printf("  mov eax, DWORD PTR [rax]\n");
    else /* size == 8 */
        printf("  mov rax, [rax]\n");
}

static char *rax_of_type(Type *type) {
    if (type->ty == TY_ARRAY)
        return "rax";
    int size = type_size(type);
    if (size == 1)
        return "al";
    if (size == 4)
        return "eax";
    return "rax";
}

static char *rdi_of_type(Type *type) {
    if (type->ty == TY_ARRAY)
        return "rdi";
    int size = type_size(type);
    if (size == 1)
        return "dil";
    if (size == 4)
        return "edi";
    return "rdi";
}
