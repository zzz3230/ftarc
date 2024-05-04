#include <strings.h>
#include "ustring.h"

Str str(char* text){
    Str str;
    str.length = (int)strlen(text);
    str.value = text;
    return str;
}

Str str_concat(Str a, Str b){
    Str s;
    s.value = malloc(a.length + b.length + 1);
    strcpy(s.value, a.value);
    strcpy(s.value + a.length, b.value);
    s.value[a.length + b.length] = 0;
    s.length = a.length + b.length;
    return s;
}

Str str_empty(int len){
    Str s;
    s.length = len;
    s.value = calloc(len + 1, sizeof(char));
    return s;
}

Str str_copy(Str source){
    Str s = str_empty(source.length);
    strcpy(s.value, source.value);
    return s;
}