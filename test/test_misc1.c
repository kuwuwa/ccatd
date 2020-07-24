void fail() {
    assert_equals(0, 1);
}

int test_return1() {
    int a = 2;
    int b = a + 4;
    int c = 0;
    return c = a * (b + 1) + 5;
    a + b + c;
}

int test_return2() {
    1;
    2;
    3;
    return 4;
    return 5;
}

int calc1(int x, int y) {
    return x * x + y * y;
}

int f(int x) {
    return 10 - x;
}

int g(int x) {
    return x*4;
}

int h(int x, int y) {
    int z = x + g(x + y);
    return z;
}

int fib(int x) {
    if(x <= 1) return x;
    return fib(x-1) + fib(x-2);
}

int inc(int* x) {
    *x = *x+1;
}

int global1;
int global2;
int* global2a = &global2 - 1;
int global3[10];

int main() {
    { // arith1
        int num = 1 + -2 * -3 + 4;
        assert_equals(121, num * num);
    }
    { // arith2
        int pppp = 100 + (20 - 3);
        assert_equals(117, pppp);
    }
    // arith3
    assert_equals(1 * 2 * 3 * 4 * 5 == 120, 1);
    // arith4
    assert_equals(1 < 2 > 0 != 0, 1);
    // return1
    assert_equals(test_return1(), 19);
    // return2
    assert_equals(test_return2(), 4);
    { // assignment1
        int a = 0;
        if (-1) a = 8;
        else a = 4;
        assert_equals(a, 8);
    }
    { // assignment2
        int a = 0;
        if (0) a = 8;
        else a = 4;
        assert_equals(a, 4);
    }
    { // assignment3
        int x = 3;
        int y = 4;
        assert_equals(x + (x + y), 10);
    }
    { // ifelse1
        int a = 3;
        if (a < 2) a=2;
        else a = 5;

        int b = 10;
        if (a < b) b=100;
        assert_equals(a+b, 105);
    }
    { // while1
        int a = 11;
        int sum = 0;
        while (a > 0) sum = sum + (a = a-1);
        assert_equals(sum, 55);
    }
    { // while2
        int a = 10;
        while (1 > 2) a = 100;
        assert_equals(a, 10);
    }
    { // for1
        int b = 1;
        for (int a = 1; a <= 5; a = a+1) b = b * a;
        assert_equals(b, 120);
    }
    { // call1
        int a = 5;
        assert_equals(a * bar(a - 9, 2 - a, -2), 70); // bar is defined in runtime
    }
    // fundef1
    assert_equals(calc1(2, 3), 13);
    // fundef2
    assert_equals(10 * f(8), 20);
    // fundef3
    assert_equals(h(10, 9), 86);
    // fundef4
    assert_equals(fib(10), 55);
    { // pointer1
        int a = 10;
        int b = 20;
        int* c = &b + 1;
        assert_equals(*c, 10);
    }
    { // pointer2
        int a = 0;
        inc(&a);
        inc(&a);
        inc(&a);
        assert_equals(a, 3);
    }
    { // pointer3
        int x = 10;
        int* y = &x;
        *y = 20;
        assert_equals(x, 20);
    }
    { // pointer4
        int a[4] = {1, 2, 4, 8};
        *(a+1) = 4294967295;
        *(a+3) = 4294967295;
        assert_equals(a[2], 4);
    }
    // sizeof1
    assert_equals(sizeof(1), 4);
    // sizeof2
    assert_equals(sizeof sizeof 10, 4);
    { // sizeof3
        int *x;
        assert_equals(sizeof x, 8);
    }
    { // array1
        int a[3];
        assert_equals(sizeof a, 12);
    }
    { // array2
        int a[4];
        *(a+1) = 10;
        assert_equals(a[1], 10);
    }
    { // array3
        int a[2];
        *a = 1;
        *(a + 1) = 2;
        int *p = a;
        assert_equals(*p + *(p + 1), 3);
    }
    global1 = 9;
    assert_equals(global1, 9);
    assert_equals(*global2a, 9);
    global2 = 2;
    global3[8] = 10;
    assert_equals(global2 * global3[8], 20);
    { // char1
        char str[8];
        str[2] = 1;
        str[1] = 255;
        str[3] = 255;
        assert_equals(str[2], 1);
    }
    { // string1
        char *str = "-------W-----";
        assert_equals(str[7], 87);
    }
    { // comment1
        // This is a comment /*
        int x = 111;
        // */
        assert_equals(x, 111);
    }
    { // comment2
        int y = 10;
        /*
        int y = 30;
        */
        assert_equals(y, 10);
    }
    return 0;
}
