
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
    echo "$input => $expected expected"
    exit 1
  fi
}

try 0 0
try 42 42
