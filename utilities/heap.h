#ifndef HEAP_H_
#define HEAP_H_

#include "graph.h"

#define HEAP_TYPE Node*
#define HEAP_COMPARATOR(a, b, dir) ( \
    (a)->length == (b)->length ?    \
        ((a)->value > (b)->value) : \
        ((a)->length < (b)->length) \
)

typedef struct s_heap{
    HEAP_TYPE* arr;
    int length;
    int size;
} Heap;

Heap* heap_create(int size);
void heap_push(Heap* h, HEAP_TYPE n);
HEAP_TYPE heap_pop(Heap* h);
int heap_count(Heap* h);
int heap_is_empty(Heap* h);
void heap_clear(Heap* h);
void heap_free(Heap* h);

#endif