
int fib_arr[13] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
int x = 20;
char *hello_world = "Hello, World!\n";
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

    assert_equals((1 < func() ? 10 : 20), 10);
    assert_equals((3 < 2 ? 10 : 20), 20);

    int v = 10;
    assert_equals((1, (v = 20), 3, 4), 4);
    assert_equals(v, 20);

    assert_equals(10 | 4 | 16, 30);
    assert_equals(10 & 4, 0);
    assert_equals(14 ^ 7 ^ 6, 15);

    assert_equals(1 << 2 << 8, 1024);
    assert_equals(3072 >> 6 >> 4, 3);

    assert_equals(10 % 3, 1);
    assert_equals((-10) % 3, -1);

    int s = 0;
    for (int i = 0; i <= 5; i = i+1) s = s + i;
    assert_equals(s, 15);

    char str[5];
    str[0] = 'c';
    str[1] = 'h';
    str[2] = 'a';
    str[3] = 'r';
    str[4] = 0;
    print(str);

    return 0;
}
