#include <stdio.h>
#include <stdlib.h>

void foo() {
    printf("OK\n");
}

int bar(int x, int y, int z) {
    return x * y - z;
}

void alloc4(int ** arr, int v1, int v2, int v3, int v4) {
    *arr = (int*) calloc(4, sizeof(int));
    (*arr)[0] = v1;
    (*arr)[1] = v2;
    (*arr)[2] = v3;
    (*arr)[3] = v4;
}

void print(char *str) {
    printf("%s", str);
}

void assert_equals(int v1, int v2) {
    if (v1 != v2) {
        fprintf(stderr, "expected: %d, actual: %d\n", v2, v1);
        exit(1);
    }
}
