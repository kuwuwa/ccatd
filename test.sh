
CC=gcc
APP=ccatd

try() {
  expected="$1"
  input="$2"
  ./${APP} "{ $input }" > _temp.s
  if [ "$?" != 0 ]; then
    echo "compilation failed: \"$2\""
    exit 1
  fi
  ${CC} -o _temp _temp.s
  ./_temp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but actually $actual"
    exit 1
  fi
}

try 121 'num = 1 + - 2  * - 3  +  4; num * num;'
try 117 'pppp = 100 + (20 - 3); pppp;'
try 1 '1 * 2 * 3 * 4 * 5 == 120;'
try 1 '1 < 2 > 0 != 0;'
try 19 'a = 2; b = a + 4; return c = a * (b + 1) + 5; a + b + c;'
try 4 '1; 2; 3; return 4; 5;'
try 8 'if (-1) 8; else 4;'
try 4 'if (0) 8; else 4;'
try 105 'a = 3; if (a < 2) a = 2; else a = 5; b = 10; if (a < b) b = 100; a+b;'
try 55 'a = 11; sum = 0; while (a > 0) sum = sum + (a = a - 1); sum;'
try 10 'a = 10; while (1 > 2) a = 100; a;'
try 120 'b = 1; for (a = 1; a <= 5; a = a + 1) b = b * a; b;'
echo "Accepted!!"
