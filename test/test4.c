
void fail() {
    assert_equals(0, 1); } 
void try_void_return() {
    return;
    fail();
}

int fun1();

extern int glov1;

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
    do { x = 1; continue; } while (0);
    assert_equals(x, 1);

    do { break; x = 2; } while (0);
    assert_equals(x, 1);

    for (;;) {
        break;
        fail();
    }

    int r = 0;
    for (int i = 0; i < 10; i = i + 1) {
        if (i % 2 == 0)
            continue;
        r = r | (1 << i);
    }
    assert_equals(r, 512+128+32+8+2);

    {
        int i = 0;
        while (i < 100) {
            i = i+1;
            if (i % 2 == 0)
                continue;
            assert_equals(i % 2, 1);
        }
    }

    try_void_return();
    try_void_return();
    try_void_return();

    void *xxx;

    assert_equals(glov1, 12345);
    assert_equals(fun1(), 0);

    int ia = 0;
    int ib = 2;
    assert_equals(++ia, --ib);
    assert_equals(ia, 1);
    assert_equals(ib--, 1);
    assert_equals(ib, 0);

    int arr[3] = {1, 10, 100};
    int *p = arr;
    assert_equals(*(p++), 1);
    assert_equals(*(p++), 10);
    assert_equals(*(p++), 100);
    p = arr;
    assert_equals(*(++p), 10);
    assert_equals(*(++p), 100);
    assert_equals(*(--p), 10);
    assert_equals(*(--p), 1);
    return 0;
}

int glov1 = 12345;

int fun1() {
    return 0;
}

