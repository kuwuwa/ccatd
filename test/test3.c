
struct Foo {
    int a;
    char *name;
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

    return 0;
}
