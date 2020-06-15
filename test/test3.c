
struct Foo {
    char *name;
    int a;
};

struct Bar {
    int a[10];
    char b[10];
};

int bar_total(struct Bar *bar) {
    int tot = 0;
    for (int i = 0; i < 10; i = i+1)
        tot = tot + bar->a[i];
    return tot;
}

typedef struct Foo Foo;

Foo global_foo_arr[20];

int main() {
    assert_equals(sizeof(global_foo_arr), 240);

    Foo foo;
    assert_equals(sizeof(foo), 12);

    struct Bar bar;
    assert_equals(sizeof(bar), 50);

    struct Foo foo_arr[12];
    assert_equals(sizeof(foo_arr), 144);

    foo.a = 10;
    assert_equals(foo.a, 10);

    (bar.b)[3] = 35;
    assert_equals(bar.b[3], 35);

    Foo* foo_ptr = &foo;
    assert_equals(foo_ptr->a, 10);
    foo_ptr->a = 20;
    assert_equals(foo.a, 20);

    struct Bar *bar_ptr = &bar;
    assert_equals(bar_ptr->b[3], 35);
    bar_ptr->b[3] = 123;
    assert_equals(bar.b[3], 123);

    for (int i = 0; i < 10; i = i+1)
        bar.a[i] = i;

    assert_equals(bar_total(bar_ptr), 45);

    assert_equals(sizeof(struct Bar*), 8);

    return 0;
}
