#ifndef USTRING_H_
#define USTRING_H_

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
Str str_concat_path(Str path1, Str path2);
int str_equals(Str a, Str b);
void str_free(Str s);

#endif