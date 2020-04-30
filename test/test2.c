
int fib_arr[10] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
int x = 20;
char *hello_world = "Hello, World!";
int *y = &x;

int main() {
    //
    assert_equals(fib_arr[3], 2);
    assert_equals(fib_arr[5], 5);
    assert_equals(fib_arr[6], 8);

    print(hello_world);

    *y = *y + 10;
    assert_equals(x + x + x, 90);

    return 0;
}
