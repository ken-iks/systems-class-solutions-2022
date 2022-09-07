#include <cstdio>
#include <iostream>
using namespace std;

int main(int argc, char* argv[]){
    int i = 1;
    while (i < argc) {
        int j = i;
        while (j > 0 && strcmp(argv[j-1], argv[j]) > 0) {
            char* temp = argv[j];
            argv[j] = argv[j-1];
            argv[j-1] = temp;
            j--;
        }
        i++;
    }
    for (int j = 1; j < argc; j++) {
        printf("%s\n", argv[j]);
    };
    return 0;
}