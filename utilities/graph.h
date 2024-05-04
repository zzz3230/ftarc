#ifndef GRAPH_H_
#define GRAPH_H_

#include <stdint.h>

typedef struct s_node{
    int value;
    int64_t length;
    struct s_node* left;
    struct s_node* right;
} Node;

int node_length(Node* node);

#endif