int g(int x) {
    return x*4;
}

int h(int x, int y) {
    int z = x + f(x+y);
    return z;
}

int main() {
    return h(10,9);
}
