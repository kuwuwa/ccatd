
void *calloc(long nmemb, long size);

typedef struct E E;

struct E {
    int v;
    E *next;
    int u;
};

E *E_new(int v, E *next) {
    E *e = calloc(1, sizeof(E));
    e->v = v;
    e->next = next;
    if (next != 0)
        next->u++;
    return e;
}

int E_idx(E* e, int idx) {
    if (idx == 0)
        return e->v;
    return E_idx(e->next, idx-1);
}

int main() {
    E *e1 = E_new(1, 0);
    E *e2 = E_new(2, e1);
    E *e3 = E_new(3, e2);
    E *e4 = E_new(4, e3);
    E *e5 = E_new(5, e4);
    E *e6 = E_new(6, e5);

    assert_equals(E_idx(e6, 4), 2);

    return 0;
}
