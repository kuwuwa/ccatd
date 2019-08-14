
CC=gcc
APP=ccatd

try() {
  expected="$1"
  input="$2"
  ./${APP} "$input" > _temp.s
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

try 11 '1  +  2  *  3  +  4'
try 106 '100 + 20 / 3'
try 117 '100 + (20 - 3)'
