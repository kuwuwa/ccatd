#include "ccatd.h"

static void append_type_param(Vec *params, Type* t) {
    Node *node = calloc(1, sizeof(Node));
    node->type = t;
    node->kind = ND_VAR;
    vec_push(params, node);
}

static void push_function(char *name, Type **arg_types, int argc, Type *ret_type, bool is_varargs) {
    Func *func = calloc(1, sizeof(Func));
    func->name = name;
    func->is_varargs = is_varargs;
    func->params = vec_new();
    for (int i = 0; i < argc; i++)
        append_type_param(func->params, arg_types[i]);
    func->ret_type = ret_type;

    map_put(func_env, name, func);
}

static Type *mktype(Type_kind kind, Type *ptr_to) {
    Type *typ = calloc(1, sizeof(Type));
    typ->ty = kind;
    typ->ptr_to = ptr_to;
    return typ;
}

static void init() {
    // type

    type_void = mktype(TY_VOID, NULL);
    type_int = mktype(TY_INT, NULL);
    type_char = mktype(TY_CHAR, NULL);
    type_ptr_char = ptr_of(type_char);

    builtin_aliases = env_new(NULL);
    env_push(builtin_aliases, "void", type_void);
    env_push(builtin_aliases, "int", type_int);
    env_push(builtin_aliases, "char", type_char);

    env_push(builtin_aliases, "bool", type_char); // 1 bit
    env_push(builtin_aliases, "long", type_int); // 8 bit
    env_push(builtin_aliases, "size_t", type_int); // 8 bit


    // parse

    functions = vec_new();
    global_vars = map_new();
    string_literals = vec_new();

    // semantic

    func_env = map_new();
    push_function("foo", NULL, 0, type_void, false);

    Type* bar_args[3] = {type_int, type_int, type_int};
    push_function("bar", bar_args, 3, type_int, false);

    Type *print_args[1] = {ptr_of(type_char)};
    push_function("print", print_args, 1, type_void, false);

    Type *assert_equals_args[2] = {type_int, type_int};
    push_function("assert_equals", assert_equals_args, 2, type_int, false);

    Type *builtin_va_start_args[1] = {type_void};
    push_function("__builtin_va_start", builtin_va_start_args, 1, type_void, true);
}

static char *read_file(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        error("cannot open %s: %d\n", path, strerror(errno));

    if (fseek(fp, 0, SEEK_END) == -1)
        error("%s: fseek: %s", path, strerror(errno));

    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) == -1)
        error("%s: fseek: %s", path, strerror(errno));

    char *buf = calloc(1, size + 2);
    fread(buf, size, 1, fp);

    if (size == 0 || buf[size - 1] != '\n') {
        buf[size] = '\n';
        size++;
    }
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid number of argument(s)\n");
        return 1;
    }
    init();

    char *code = read_file(argv[1]);
    tokenize(code);
    parse();

    sema_globals();

    for (int i = 0; i < vec_len(functions); i++) {
        Func *func = vec_at(functions, i);
        sema_func(func);
    }

    printf("  .intel_syntax noprefix\n"
           "  .globl main\n");

    gen_globals();

    int len = vec_len(functions);
    for (int i = 0; i < len; i++) {
        Func *func = vec_at(functions, i);
        if (!func->is_static)
            printf("  .globl %s\n", func->name);
    }

    for (int i = 0; i < len; i++) {
        gen_func(vec_at(functions, i));
    }
    return 0;
}

