

int main() {
    int brr[10];
    int idx = 0;
    while (idx < 10)
        brr[idx++] = 100;
    for (int i = 0; i < 10; i++)
        assert_equals(brr[i], 100);
    return 0;
}

