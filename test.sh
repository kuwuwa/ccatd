
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

try 1 'x = 1;'
try 121 'x = 1 + 2  *  3  +  4; x * x;'
try 27 'a = 2; b = a + 4; c = a * (b + 1) + 5; a + b + c;'
try 117 'p = 100 + (20 - 3); p;'
try 23 'q = - 10 * - 2 + 3;'
try 1 '1 * 2 * 3 * 4 * 5 == 120;'
try 1 '1 < 2 > 0 != 0;'
echo "Accepted!!"
