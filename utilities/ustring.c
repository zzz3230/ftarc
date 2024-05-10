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

Str str_concat_path(Str path1, Str path2){
    if(path1.value[path1.length - 1] == '/' || path1.value[path1.length - 1] == '\\'){
        return str_concat(path1, path2);
    }
    Str norm_path1 = str_concat(path1, str("/"));
    Str res_path = str_concat(norm_path1, path2);
    str_free(norm_path1);
    return res_path;
}

int str_equals(Str a, Str b){
    if(a.length == b.length){
        return strcmp(a.value, b.value) == 0;
    }
    return 0;
}

void str_free(Str s){
    free(s.value);
}