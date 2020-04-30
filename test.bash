
CC=gcc
CFLAGS='-static -g'
APP=ccatd

try_return() {
  filename="$1"
  expected="$2"
  ./${APP} "$filename" > _temp.s
  if [ "$?" != 0 ]; then
    echo "compilation failed: ${filename}"
    exit 1
  fi
  ${CC} ${CFLAGS} -o _temp runtime.o _temp.s
  if [ "$?" != 0 ]; then
    echo "link failed: \"$2\""
    exit 1
  fi
  ./_temp > /dev/null
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
  if [ "$?" != 0 ]; then
    echo "compilation failed: ${filename}"
    exit 1
  fi
  ${CC} ${CFLAGS} -o _temp runtime.o _temp.s
  ./_temp > _temp.txt
  actual=$(cat _temp.txt)

  if [ "$actual" != "$expected" ]; then
    echo "\${filename} => $expected expected, but actually $actual"
    exit 1
  fi
}

try_stdout 'sample/call2.c' 'OK'
try_stdout 'sample/char2.c' 'Hello, World!'
try_stdout 'sample/string2.c' 'hack'

try_return 'test/test1.c' 0
try_return 'test/test2.c' 0

echo "Accepted!!"
