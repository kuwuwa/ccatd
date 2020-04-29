int inc(int* x){*x=*x+1;} int main(){ int a = 0; inc(&a); inc(&a); inc(&a); return a; }
