
CC=gcc
APP=ccatd

try() {
  expected="$1"
  input="$2"
  ./${APP} "$input" > _temp.s
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

try 42 'answer = 42;'
try 121 'num = 1 + 2  *  3  +  4; num * num;'
try 117 'pppp = 100 + (20 - 3); pppp;'
try 23 'qqqq = - 10 * - 2 + 3;'
try 1 '1 * 2 * 3 * 4 * 5 == 120;'
try 1 '1 < 2 > 0 != 0;'
try 27 'a = 2; b = a + 4; c = a * (b + 1) + 5; a + b + c;'
try 19 'a = 2; b = a + 4; return c = a * (b + 1) + 5; a + b + c;'
try 4 '1; 2; 3; return 4; 5;'
echo "Accepted!!"
