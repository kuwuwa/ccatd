
int main() {
    int a = 0;
    int b = 1;
    int n = 0;
    do {
        int t = b;
        b = a + b;
        a = t;
        n = n + 1;
    } while (n < 9);
    assert_equals(b, 55);

    int x = 0;
    do { x = 1; } while (0);
    assert_equals(x, 1);

    return 0;
}
