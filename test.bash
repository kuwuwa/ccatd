
CC=gcc
CFLAGS='-static -g'
APP=ccatd

try_return() {
  filename="$1"
  expected="$2"
  ./${APP} "$filename" > _temp.s
  if [ "$?" != 0 ]; then
    echo "compilation failed: \"$2\""
    exit 1
  fi
  ${CC} ${CFLAGS} -o _temp runtime.o _temp.s
  if [ "$?" != 0 ]; then
    echo "link failed: \"$2\""
    exit 1
  fi
  ./_temp
  actual="$?"

  if [ "$actual" != "$expected" ]; then
    echo "$filename => $expected expected, but actually $actual"
    exit 1
  fi
}

try_stdout() {
  filename="$1"
  expected="$2"
  ./${APP} "${filename}" > _temp.s
  ${CC} ${CFLAGS} -o _temp runtime.o _temp.s
  ./_temp > _temp.txt
  actual=$(cat _temp.txt)

  if [ "$actual" != "$expected" ]; then
    echo "\${filename} >> $expected expected, but actually $actual"
    exit 1
  fi
}

try_return 'sample/arith1.c' '121'
try_return 'sample/arith2.c' '117'
try_return 'sample/arith3.c' '1'
try_return 'sample/arith4.c' '1'
try_return 'sample/return1.c' '19'
try_return 'sample/return2.c' '4'
try_return 'sample/assignment1.c' '8'
try_return 'sample/assignment2.c' '4'
try_return 'sample/assignment3.c' '10'
try_return 'sample/ifelse1.c' '105'
try_return 'sample/while1.c' '55'
try_return 'sample/while2.c' '10'
try_return 'sample/for1.c' '120'
try_return 'sample/call1.c' '70'
try_stdout 'sample/call2.c' 'OK'
try_return 'sample/fundef1.c' '13'
try_return 'sample/fundef2.c' '20'
try_return 'sample/fundef3.c' '86'
try_return 'sample/fundef4.c' '55'
try_return 'sample/pointer1.c' '10'
try_return 'sample/pointer2.c' '3'
try_return 'sample/pointer3.c' '20'
try_return 'sample/pointer4.c' '4'
try_return 'sample/sizeof1.c' '4'
try_return 'sample/sizeof2.c' '4'
try_return 'sample/sizeof3.c' '8'
try_return 'sample/array1.c' '12'
try_return 'sample/array2.c' '10'
try_return 'sample/array3.c' '3'
try_return 'sample/global1.c' '9'
try_return 'sample/global2.c' '20'
try_return 'sample/char1.c' '1'
try_stdout 'sample/char2.c' 'Hello, World!'
try_return 'sample/string1.c' '87'
try_stdout 'sample/string2.c' 'hack'

echo "Accepted!!"
