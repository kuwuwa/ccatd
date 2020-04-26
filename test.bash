
CC=gcc
APP=ccatd

try_return() {
  expected="$1"
  input="$2"
  ./${APP} "$input" > _temp.s
  if [ "$?" != 0 ]; then
    echo "compilation failed: \"$2\""
    exit 1
  fi
  ${CC} -g -no-pie -o _temp runtime.o _temp.s
  if [ "$?" != 0 ]; then
    echo "link failed: \"$2\""
    exit 1
  fi
  ./_temp
  actual="$?"

  if [ "$actual" != "$expected" ]; then
    echo "\`$input\` => $expected expected, but actually $actual"
    exit 1
  fi
}

try_stdout() {
  expected="$1"
  input="$2"
  ./${APP} "$input" > _temp.s
  ${CC} -o _temp runtime.o _temp.s
  ./_temp > _temp.txt
  actual=$(cat _temp.txt)

  if [ "$actual" != "$expected" ]; then
    echo "\`$input\` >> $expected expected, but actually $actual"
    exit 1
  fi
}

# arithmetic expression
try_return 121 'int main() { int num = 1 + - 2  * - 3  +  4; return num * num; }'
try_return 117 'int main() { int pppp = 100 + (20 - 3); return pppp; }'
try_return 1   'int main() { return 1 * 2 * 3 * 4 * 5 == 120; }'
try_return 1   'int main() { return 1 < 2 > 0 != 0; }'
# return
try_return 19 'int main() { int a = 2; int b = a + 4; int c = 0; return c = a * (b + 1) + 5; a + b + c; }'
try_return 4  'int main() { 1; 2; 3; return 4; 5; }'
# assignment
try_return 8  'int main() { int a = 0; if (-1) a = 8; else a = 4; return a; }'
try_return 4  'int main() { int a = 0; if (0) a = 8; else a = 4; return a; }'
try_return 10 'int main() { int x = 3; int y = 4; x + (x + y); }'
# if/else
try_return 105 'int main() { int a=3; if (a<2) a=2; else a=5; int b=10; if (a<b) b=100; return a+b; }'
# while
try_return 55 'int main() { int a=11; int sum=0; while (a>0) sum=sum+(a=a-1); return sum; }'
try_return 10 'int main() { int a=10; while (1 > 2) a=100; return a; }'
# for
try_return 120 'int main() { int b = 1; int a = 0; for (a=1; a<=5; a=a+1) b=b*a; return b; }'
# function call
try_return 70 'int main() { int a = 5; return a * bar(a - 9, 2 - a, -2); }'
try_stdout "OK" 'int main() { foo(); }'
# function definition
try_return 13 'int calc1(int x,int y) { return x * x + y * y; } int main(){return calc1(2, 3);}'
try_return 20 'int f(int x) { return 10-x; } int main() { return 10*f(8); }'
try_return 86 'int f(int x){return x*4;} int g(int x,int y){int z=x+f(x+y); return z;} int main(){return g(10,9);}'
try_return 55 'int fib(int x){if(x<=1)return x;return fib(x-1)+fib(x-2);} int main(){return fib(10);}'
# pointer
try_return 10 'int main() { int a = 10; int b = 20; int* c = &b + 1; return *c; }'
try_return 3  'int inc(int* x){*x=*x+1;} int main(){ int a = 0; inc(&a); inc(&a); inc(&a); return a; }'
try_return 20 'int main() { int x = 10; int* y = &x; *y = 20; return x; }'
# addition/subtraction of pointer
try_return 4 'int main() { int* a; alloc4(&a, 1, 2, 4, 8); *(a+1) = 4294967295; *(a+3)=4294967295; return *(a+2); }'
# sizeof
try_return 4 'int main() { return sizeof(1); }'
try_return 4 'int main() { return sizeof sizeof 10; }'
try_return 8 'int main() { int *x; return sizeof x; }'
# array
try_return 12 'int main() { int a[3]; return sizeof(a); }'
try_return 10 'int main() { int a[4]; *(a+1)=10; return a[1]; }'
try_return 3 'int main() { int a[2]; *a = 1; *(a + 1) = 2; int *p = a; return *p + *(p + 1); }'
# global variable
try_return 9 'int x; int main() { x = 9; return x; }'
try_return 20 'int x; int y[10]; int main() { x = 2; y[8] = 10; return x * y[8];}'
echo "Accepted!!"
