
struct FILE;
typedef struct FILE FILE;
extern FILE *stdout;
extern FILE *stderr;

int exit(int code);

void *calloc(long nmemb, long size);

FILE *fopen(char *pathname, char *mode);
int fseek(FILE *fp, int size, int mode);
int fread(void *ptr, long size, long nmemb, FILE *stream);
size_t ftell(FILE *fp);
void fclose(FILE *fp);

long strlen(char *p);

int printf(char *fmt, ...);

int seek_set = 0;
int seek_end = 2;

static char *read_file(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        exit(1);

    if (fseek(fp, 0, seek_end) == -1)
        exit(1);

    size_t size = ftell(fp);
    assert_equals(size, 13);
    if (fseek(fp, 0, seek_set) == -1)
        exit(1);

    char *buf = calloc(1, size + 2);
    fread(buf, size, 1, fp);

    if (size == 0 || buf[size - 1] != '\n') {
        buf[size++] = '\n';
    }
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

// static char *read_file(char *path) {
//     FILE *fp = fopen(path, "r");
//     if (!fp)
//         error("cannot open %s: %d\n", path, strerror(errno));
// 
//     if (fseek(fp, 0, SEEK_END) == -1)
//         error("%s: fseek: %s", path, strerror(errno));
// 
//     size_t size = ftell(fp);
//     if (fseek(fp, 0, SEEK_SET) == -1)
//         error("%s: fseek: %s", path, strerror(errno));
// 
//     char *buf = calloc(1, size + 2);
//     fread(buf, size, 1, fp);
// 
//     if (size == 0 || buf[size - 1] != '\n') {
//         buf[size++] = '\n';
//     }
//     buf[size] = '\0';
//     fclose(fp);
//     return buf;
// }

int main() {
    char *text = read_file("./test/test_file_input.txt");
    printf("%s", text);
    return 0;
}
