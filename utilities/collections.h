#ifndef COLLECTIONS_H_
#define COLLECTIONS_H_


#include "ustring.h"
#include "stdlib.h"
#include "assert.h"
#include "../archive_structs.h"
#include "../exceptions.h"

#define DYN_LIST_IMPL_GENERATOR(type, name, prefix, comparator)\
name* prefix##_create(int capacity){\
    name* l = calloc(1, sizeof(name));\
    l->arr = calloc(capacity, sizeof(type));\
    l->capacity = capacity;\
    l->count = 0;\
    return l;\
}\
void prefix##_append(name* list, type val){\
    if(list->count >= list->capacity){\
        if(list->capacity == 0){\
            list->capacity = 1;\
        }                                      \
        list->arr = realloc(list->arr, list->capacity * sizeof(type) * 2);\
        list->capacity *= 2;\
        if(list->arr == NULL){\
            u_assert(0 && "realloc failed");\
        }\
    }\
    list->arr[list->count] = val;\
    list->count++;\
}\
type prefix##_get(name* list, int index){\
    if(index < 0 || index >= list->count){\
        u_assert(0 && "index out of range");\
    }\
    return list->arr[index];\
}                                              \
type* prefix##_get_ref(name* list, int index){\
    if(index < 0 || index >= list->count){\
        u_assert(0 && "index out of range");\
    }\
    return &list->arr[index];\
}                                              \
type prefix##_last(name* list){\
    return list->arr[list->count - 1];\
}                                               \
void prefix##_free(name* list){\
    free(list->arr);\
    free(list);\
}\
void prefix##_clear(name* list){\
    list->count = 0;\
}                                                              \
                                                               \
int prefix##_find(name* list, type value){      \
    for(int i = 0; i < list->count; i++){           \
        if(comparator(list->arr[i], value)){                    \
            return i;                                                        \
        }                                                \
    }                                                           \
    return -1;\
}\
void prefix##_remove_at(name* list, int index){\
    for(int i = index; i < list->count - 1; i++){           \
        list->arr[i] = list->arr[i+1];\
    }                                                          \
    list->count--;                                                               \
}

#define DYN_LIST_HEADER_GENERATOR(type, name, prefix) \
typedef struct s_##name{\
    type* arr;\
    int capacity;\
    int count;\
} name;\
name* prefix##_create(int capacity);\
void prefix##_append(name* list, type val);\
type prefix##_get(name* list, int index);\
type* prefix##_get_ref(name* list, int index);\
type prefix##_last(name* list);\
void prefix##_free(name* list);\
void prefix##_clear(name* list);                      \
int prefix##_find(name* list, type value);            \
void prefix##_remove_at(name* list, int index)

DYN_LIST_HEADER_GENERATOR(Str, DynListStr, dl_str);
DYN_LIST_HEADER_GENERATOR(int, DynListInt, dl_int);
DYN_LIST_HEADER_GENERATOR(ArchiveFile, DynListArchiveFile, dl_arc_file);

#endif