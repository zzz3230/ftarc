#include "stdlib.h"
#include "utils.h"

typedef struct s_str{
    char* value;
    int length;
    int capacity;
} Str;

Str str(char* text);
Str str_concat(Str a, Str b);
Str str_empty(int len);
Str str_copy(Str source);