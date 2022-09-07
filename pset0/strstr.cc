#include <cstring>
#include <cassert>
#include <cstdio>


char* mystrstr1(const char* s1, const char* s2) {
    // loop over `s1` (C strings end at the character with value 0)
    for (size_t i = 0; s1[i] != 0; ++i) {
        // loop over `s2` until `s2` ends or differs from `s1`
        size_t j = 0;
        while (s2[j] != 0 && s2[j] == s1[i + j]) {
            ++j;
        }
        // success if we got to the end of `s2`
        if (!s2[j]) {
            return (char*) &s1[i];
        }
    }
    // special case
    if (!s2[0]) {
        return (char*) s1;
    }
    return nullptr;
}

char* mystrstr2(const char* s1, const char* s2) {
    // loop over `s1`
    while (*s1) {
        // loop over `s2` until `s2` ends or differs from `s1`
        const char* s1try = s1;
        const char* s2try = s2;
        while (*s2try && *s2try == *s1try) {
            ++s2try;
            ++s1try;
        }
        // success if we got to the end of `s2`
        if (!*s2try) {
            return (char*) s1;
        }
        ++s1;
    }
    // special case
    if (!*s2) {
        return (char*) s1;
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    assert(argc == 3);
    printf("strstr(\"%s\", \"%s\") = %p\n",
           argv[1], argv[2], strstr(argv[1], argv[2]));
    printf("mystrstr(\"%s\", \"%s\") = %p\n",
           argv[1], argv[2], mystrstr1(argv[1], argv[2]));
    assert(strstr(argv[1], argv[2]) == mystrstr1(argv[1], argv[2]));
}
