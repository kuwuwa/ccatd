#include "ccatd.h"

void foo() {
    printf("OK\n");
}

int bar(int x, int y, int z) {
    return x * y - z;
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
