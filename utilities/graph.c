#include <stdlib.h>
#include "graph.h"

int node_length(Node* node){
    if(node == NULL)
        return 0;
    return node->length;
}