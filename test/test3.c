
struct Foo {
    char *name;
    int a;
};

struct Bar {
    int a[10];
    char b[10];
};

struct Foo global_foo_arr[20];

int main() {
    assert_equals(sizeof(global_foo_arr), 240);

    struct Foo foo;
    assert_equals(sizeof(foo), 12);

    struct Bar bar;
    assert_equals(sizeof(bar), 50);

    struct Foo foo_arr[12];
    assert_equals(sizeof(foo_arr), 144);

    foo.a = 10;
    assert_equals(foo.a, 10);

    (bar.b)[3] = 35;
    assert_equals(bar.b[3], 35);

    return 0;
}
