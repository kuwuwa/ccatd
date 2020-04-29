int f(int x) {
    return x*4;
}

int g(int x, int y) {
    int z = x + f(x+y);
    return z;
}

int main() {
    return g(10,9);
}
