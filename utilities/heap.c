#include <assert.h>
#include "heap.h"
#include "stdlib.h"

int heap_left(int i){
    return 2 * i + 1;
}
int heap_right(int i){
    return 2 * i + 2;
}
int heap_parent(int i){
    return (i - 1) / 2;
}
void swap_heap_type(HEAP_TYPE* a, HEAP_TYPE* b){
    HEAP_TYPE tmp = *a;
    *a = *b;
    *b = tmp;
}

Heap* heap_create(int size){
    Heap* h = calloc(1, sizeof(Heap));
    assert(h);
    h->arr = calloc(size, sizeof(HEAP_TYPE));
    h->size = size;
    return h;
}
void heap_siftup(Heap* h, int i){
    if(i == 0){
        return;
    }
    if(HEAP_COMPARATOR(h->arr[i], h->arr[heap_parent(i)], 1)){
        swap_heap_type(&h->arr[i], &h->arr[heap_parent(i)]);
        heap_siftup(h, heap_parent(i));
    }
}
void heap_siftdown(Heap* h, int i){
    int l = heap_left(i);
    int r = heap_right(i);

    int smallest = i;
    if(l < h->length && l < h->size && HEAP_COMPARATOR(h->arr[l], h->arr[i], 0)){
        smallest = l;
    }
    if(r < h->length && r < h->size && HEAP_COMPARATOR(h->arr[r], h->arr[smallest], 0)){
        smallest = r;
    }
    if(smallest != i){
        swap_heap_type(&h->arr[i], &h->arr[smallest]);
        heap_siftdown(h, smallest);
    }
}

void heap_push(Heap* h, HEAP_TYPE n){
    if(h->length >= h->size) {
        assert(0 && "heap is full");
    }
    h->arr[h->length] = n;
    h->length++;
    heap_siftup(h, h->length - 1);
}
HEAP_TYPE heap_pop(Heap* h){
    HEAP_TYPE res = h->arr[0];
    h->arr[0] = h->arr[h->length-1];
    HEAP_TYPE empty = {0};
    h->arr[h->length-1] = empty;
    h->length--;
    heap_siftdown(h, 0);
    return res;
}
int heap_is_empty(Heap* h){
    return h->length == 0;
}
void heap_clear(Heap* h){
    h->length = 0;
}
int heap_count(Heap* h){
    return h->length;
}
void heap_free(Heap* h){
    free(h->arr);
    free(h);
}