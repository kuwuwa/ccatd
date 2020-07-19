
CC=gcc
CFLAGS='-static -g'
# APP=ccatd

try_return() {
  filename="$1"
  expected="$2"
  echo "running ${1}..."
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
  echo "running ${1}..."
  ./${APP} "${filename}" > _temp.s
  if [ "$?" != 0 ]; then
    echo "compilation failed: ${filename}"
    exit 1
  fi
  ${CC} ${CFLAGS} -o _temp runtime.o _temp.s
  ./_temp > _temp.txt
  actual=$(cat _temp.txt)

  if [ "$actual" != "$expected" ]; then
    echo "${filename} => $expected expected, but actually $actual"
    exit 1
  fi
}

try_stdout 'sample/call2.c' 'OK'
try_stdout 'sample/char2.c' "Hello, World!"
try_stdout 'sample/string2.c' '"hack"'
try_stdout 'sample/string3.c' 'char'
try_return 'sample/assignment2.c' 4

try_return 'test/test_misc1.c' 0
try_return 'test/test_misc2.c' 0
try_return 'test/test_operators.c' 0
try_return 'test/test_struct.c' 0
try_stdout 'test/test_variadic.c' 'abcXYZabc12345'
try_return 'test/test_list.c' 0
try_return 'test/test_incr.c' 0
try_stdout 'test/test_file.c' 'this is text'

echo "All tests passed"
