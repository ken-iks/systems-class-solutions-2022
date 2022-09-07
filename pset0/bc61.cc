#include <cstdio>

int main() {
    unsigned long size = 0;
    while (fgetc(stdin) != EOF) {
        ++size;
    }
    fprintf(stdout, "%lu\n", size);
}