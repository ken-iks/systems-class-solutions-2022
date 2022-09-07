#include <cstdio>
#include <cctype>

int main() {
    unsigned long spaces = 0;
    unsigned long lines = 0;
    unsigned long bytes = 0;


    bool in_space = true;

    while (true) {
        int chr = fgetc(stdin);
        if (chr == (EOF)) {
            break;
        }
        bool space = isspace(chr);
        if (!space && (in_space)) {
            spaces++;
        }
        in_space = space;
        if (chr == '\n') {
            lines++;
        }
        ++bytes;
    }
    fprintf(stdout, "Lines: %lu\nWords: %lu\nBytes: %lu\n", lines, spaces, bytes);
}