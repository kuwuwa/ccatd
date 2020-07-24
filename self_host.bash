#!/bin/bash

set -e

install -d _build

process() {
  echo "processing '$1'..."
  temp_c="_build/$1"
  cat <<EOF > ${temp_c}
struct FILE;
typedef struct FILE FILE;
extern FILE *stdout;
extern FILE *stderr;

typedef struct {
  int gp_offset;
  int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
} __va_elem;

typedef __va_elem va_list[1];

static void va_start(__va_elem *ap, ...) {
  __builtin_va_start(ap);
}

void *calloc(long nmemb, long size);
void *realloc(void *ptr, long size);
void exit(int status);
int *__errno_location();

FILE *fopen(char *pathname, char *mode);
int fseek(FILE *fp, int size, int mode);
int fread(void *ptr, long size, long nmemb, FILE *stream);
size_t ftell(FILE *fp);
void fclose(FILE *fp);

int isspace(int c);
int isalpha(int c);
int isdigit(int c);
int fprintf(FILE *stream, char *fmt, ...);
int vfprintf(FILE *stream, char *fmt, va_list ap);
int printf(char *fmt, ...);
int sprintf(char *str, char *fmt, ...);

int strcmp(char *s1, char *s2);
char *strerror(int errnum);

long strlen(char *p);
int strncmp(char *p, char *q, int len);
int strncpy(char *p, char *str, int len);
int strtol(char *nptr, char **endptr, int base);
EOF

  grep -v '^#' ccatd.h >> ${temp_c}
  grep -v '^#' $1 >> ${temp_c}
  sed -i 's/\btrue\b/1/g; s/\bfalse\b/0/g;' ${temp_c}
  sed -i 's/\bNULL\b/((void*)0)/g' ${temp_c}
  sed -i 's/\berrno\b/__errno_location()/g' ${temp_c}
  sed -i 's/\bSEEK_SET\b/0/g' ${temp_c}
  sed -i 's/\bSEEK_END\b/2/g' ${temp_c}

  temp_s="_build/${1%.c}.s"
  ./ccatd ${temp_c} > ${temp_s}
  gcc -I. -g -c -o _build/${1%.c}.o ${temp_s}
}

make build

process 'codegen.c'
process 'containers.c'
process 'main.c'
process 'parse.c'
process 'semantic.c'
process 'tokenize.c'
process 'type.c'
process 'util.c'

gcc -static -g -o ccatd-ccatd _build/*.o

echo 'testing self-hosted compiler...'
APP=ccatd-ccatd ./test.bash
