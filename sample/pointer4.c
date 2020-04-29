int main() { int* a; alloc4(&a, 1, 2, 4, 8); *(a+1) = 4294967295; *(a+3)=4294967295; return *(a+2); }
