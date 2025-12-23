#ifndef CONST_H
#define CONST_H

#include <stdint.h>

#define COUNT_CHAR(str, ch) ({ \
    int count = 0; \
    for (int i = 0; str[i] != '\0'; i++) { \
        if (str[i] == ch) count++; \
    } \
    count; \
})

#define MAX_SIZE 1024

#endif // CONST_H