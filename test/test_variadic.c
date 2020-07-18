
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

void va_start(__va_elem *ap, ...) {
  __builtin_va_start(ap);
}

int strcmp(char *s1, char *s2);
int fprintf(FILE *stream, char *fmt, ...);
int vfprintf(FILE *stream, char *fmt, va_list ap);

void varargf(char *fmt, ...) {
    va_list ap;
    va_start(ap);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
}

int main() {
    if (0) {
        return 1;
    }
    varargf("abc%sabc%d", "XYZ", 12345);
    return 0;
}
