
int fib_arr[13] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
int x = 20;
char *hello_world = "Hello, World!";
int *y = &x;
int *z = fib_arr + 2;
int w;

int func() {
    return 30;
}

int main() {
    assert_equals(fib_arr[3], 2);
    assert_equals(fib_arr[5], 5);
    assert_equals(fib_arr[6], 8);

    print(hello_world);

    *y = *y + 10;
    assert_equals(x + x + x, 90);

    int poyo[3] = {1, 2, 3};
    assert_equals(poyo[2], 3);

    assert_equals(z[3], 5);

    assert_equals(0 && 1234, 0);
    assert_equals(1 && 2, 1);
    assert_equals(800 || 0, 1);
    assert_equals(0 || 0, 0);

    assert_equals(!12345, 0);
    assert_equals(!0, 1);
    assert_equals(!-389, 0);
    return 0;
}
