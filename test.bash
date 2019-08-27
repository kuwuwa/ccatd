
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
  ${CC} -o _temp runtime.o _temp.s
  if [ "$?" != 0 ]; then
    echo "link failed: \"$2\""
    exit 1
  fi
  ./_temp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "\`$input\` => $actual"
  else
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

  if [ "$actual" = "$expected" ]; then
    echo "\`$input\` >> $actual"
  else
    echo "\`$input\` >> $expected expected, but actually $actual"
    exit 1
  fi
}


try_return 121 'main() { num = 1 + - 2  * - 3  +  4; return num * num; }'
try_return 117 'main() { pppp = 100 + (20 - 3); return pppp; }'
try_return 1 'main() { return 1 * 2 * 3 * 4 * 5 == 120; }'
try_return 1 'main() { return 1 < 2 > 0 != 0; }'
try_return 19 'main() { a = 2; b = a + 4; return c = a * (b + 1) + 5; a + b + c; }'
try_return 4 'main() { 1; 2; 3; return 4; 5; }'
try_return 8 'main() { if (-1) a = 8; else a = 4; return a; }'
try_return 4 'main() { if (0) a = 8; else a = 4; return a; }'
try_return 10 'main() { x = 3; y = 4; x + (x + y); }'
try_return 105 'main() { a=3; if (a<2) a=2; else a=5; b=10; if (a<b) b=100; return a+b; }'
try_return 55 'main() { a=11; sum=0; while (a>0) sum=sum+(a=a-1); return sum; }'
try_return 10 'main() { a = 10; while (1 > 2) a = 100; return a; }'
try_return 120 'main() { b = 1; for (a=1; a<=5; a=a+1) b=b*a; return b; }'
try_return 70 'main() { a = 5; return a * bar(a - 9, 2 - a, -2); }'
try_stdout "OK" 'main() { foo(); }'
try_return 13 'calc1(x,y) { return x * x + y * y; } main(){return calc1(2, 3);}'
try_return 20 'f(x) { return 10-x; } main() { return 10*f(8); }'
try_return 86 'f(x){return x*4;} g(x,y){z=x+f(x+y); return z;} main(){return g(10,9);}'
try_return 55 'fib(x){if(x<=1)return x;return fib(x-1)+fib(x-2);} main(){return fib(10);}'
echo "Accepted!!"
