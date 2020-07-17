
struct FILE;
typedef struct FILE FILE;
extern FILE *stderr;
int fprintf(FILE *stream, char *fmt, ...);

void *calloc(int n, int s);
void *realloc(void *p, int s);

struct E {
    int v;
};

typedef struct E E;

typedef struct Vector Vec;

Vec *vec_new() {
    Vec *vec = calloc(1, sizeof(Vec));
    vec->len = 0;
    vec->cap = 8;
    vec->data = calloc(8, sizeof(E));
    return vec;
}

int vec_len(Vec* vec) {
    return vec->len;
}

void vec_push(Vec *vec, void *node) {
    if (vec->len == vec->cap) {
        vec->cap *= 2;
        vec->data = realloc(vec->data, vec->cap * sizeof(void*));
    }
    vec->data[vec->len++] = node;
}

void *vec_pop(Vec *vec) {
    if (vec->len == 0)
        return 0;

    void * ptr = vec->data[vec->len-1];
    vec->data[vec->len--] = 0;
    return ptr;
}

E *E_new(int v) {
    E *e = calloc(1, sizeof(E));
    e->v = v;
    return e;
}

struct Vector {
    int len;
    int cap;
    void **data;
};

int main() {
    Vec *vec = vec_new();
    vec_push(vec, E_new(1));
    vec_push(vec, E_new(2));
    vec_push(vec, E_new(3));
    assert_equals(vec->len, 3);

    vec_pop(vec);
    vec_pop(vec);
    vec_pop(vec);
    assert_equals(vec->len, 0);

    return 0;
}
