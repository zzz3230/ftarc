#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <synchapi.h>
#include "huffman.h"
#include "stdlib.h"
#include "../utilities/heap.h"
#include "../utilities/utils.h"


Node* l_build_tree(HuffmanCoder* coder) {
    assert(coder->intermediate_nodes == NULL);
    Node* sub_nodes = calloc(HUFF_ALPHA_LEN, sizeof(Node));
    coder->intermediate_nodes = sub_nodes;
    Heap* heap = heap_create(HUFF_ALPHA_LEN * 2);

    for (int i = 0; i < HUFF_ALPHA_LEN; ++i) {
        if(coder->symbols_nodes[i].length > 0){
            heap_push(heap, &coder->symbols_nodes[i]);
        }
    }

    int sub_node_index = 0;
    while (heap_count(heap) > 1){
        Node* a = heap_pop(heap);
        Node* b = heap_pop(heap);

        Node united;
        united.right = a;
        united.left = b;
        united.length = node_length(a) + node_length(b);
        united.value = -1;
        sub_nodes[sub_node_index++] = united;

        heap_push(heap, &sub_nodes[sub_node_index - 1]);
    }

    Node* root = heap_pop(heap);
    heap_free(heap);
    return root;
}

void l_get_code_rec(HuffmanCode* codes, Node* current, char* way, int way_i){
    if(current->value != -1){
        if(way_i != 0){
            //printf("> %d %s\n", current->value, way);
            strcpy(codes[current->value].code, way);
            codes[current->value].length = way_i;
        }
        else{
            codes[current->value].code[0] = '0';
            codes[current->value].length = 1;
        }
    }
    if(current->left){
        way[way_i] = '0';
        l_get_code_rec(codes, current->left, way, way_i + 1);
        way[way_i] = '\0';
    }
    if(current->right){
        way[way_i] = '1';
        l_get_code_rec(codes, current->right, way, way_i + 1);
        way[way_i] = '\0';
    }
}

void l_generate_symbols_nodes(HuffmanCoder* coder){
    assert(coder->symbols_nodes == NULL);

    Node* nodes = calloc(HUFF_ALPHA_LEN, sizeof(Node));

    for (int i = 0; i < HUFF_ALPHA_LEN; ++i) {
        nodes[i].value = i;
        nodes[i].length = coder->counts_table[i];
    }

    coder->symbols_nodes = nodes;
}

HuffmanCode* l_get_codes(Node* root, int n){
    HuffmanCode* codes = calloc(n, sizeof(HuffmanCode));
    char way[HUFF_CODE_MAX_LENGTH] = {0};
    l_get_code_rec(codes, root, way, 0);
    return codes;
}


HuffmanCoder* huffman_coder_create(){
    HuffmanCoder* c = calloc(1, sizeof(HuffmanCoder));
    assert(c);

    c->_read_buffer_size = HANDLE_FILE_BUFFER_SIZE;
    c->_read_buffer = malloc(c->_read_buffer_size * sizeof(char));
    assert(c->_read_buffer);

    return c;
}

// clear counts_table and free symbols_nodes, codes, intermediate_nodes
void huffman_clear(HuffmanCoder* coder){
    memset(coder->counts_table, 0, sizeof(coder->counts_table));

    free(coder->symbols_nodes);
    coder->symbols_nodes = NULL;

    free(coder->codes);
    coder->codes = NULL;

    free(coder->intermediate_nodes);
    coder->intermediate_nodes = NULL;
}

void huffman_handle_file(HuffmanCoder* coder, FILE* file){
    fseeko64(file, 0L, SEEK_END);
    int64_t file_size = ftello64(file);
    rewind(file);

    coder->read_progress = 0;

    uint64_t read_count = 0;
    uint64_t count;
    while ((count = fread(
                    coder->_read_buffer,
                    sizeof(char),
                    coder->_read_buffer_size,
                    file)
                            )){
        for (int i = 0; i < count; ++i) {
            coder->counts_table[coder->_read_buffer[i]]++;
        }
        read_count += count;
        coder->read_progress = (double)read_count / (double)file_size;
    }
    coder->read_progress = 1;
    rewind(file);
}
void huffman_build_codes(HuffmanCoder* coder){
    l_generate_symbols_nodes(coder);
    Node* root = l_build_tree(coder);
    coder->root_node = root;
    coder->codes = l_get_codes(root, HUFF_ALPHA_LEN);

//    for (int i = 0; i < HUFF_ALPHA_LEN; ++i) {
//        printf("%d %s %d\n", i,  coder->codes[i].code, coder->codes[i].length);
//    }
}


int read_bit(FILE* stream, uchar* buffer, int* bit_index){
    if(*bit_index >= 8){
        fread(buffer, sizeof(char), 1, stream);
        //printf("%d ", *buffer);
        *bit_index = 0;
    }
    //printf("%d ", (*buffer & (1 << (*bit_index))) != 0);
    return (*buffer & (1 << (*bit_index)++)) != 0;
}
Node* l_load_code(
        FILE* stream,
        uchar* buffer,
        int* bit_index,
        Node* symbols_nodes,
        Node* inter_nodes,
        int* inter_nodes_i,
        int inter_nodes_count
        ){
    if(read_bit(stream, buffer, bit_index)){
        int symbol_value = 0;
        for (int i = 0; i < 8; ++i) {
            symbol_value |= (read_bit(stream, buffer, bit_index) << i);
        }
        assert(symbol_value >= 0 && symbol_value <= 255);
        return &symbols_nodes[symbol_value];
    }
    else{
        Node* cur = &inter_nodes[(*inter_nodes_i)++];
        cur->left =
                l_load_code(stream, buffer, bit_index, symbols_nodes, inter_nodes, inter_nodes_i, inter_nodes_count);
        cur->right =
                l_load_code(stream, buffer, bit_index, symbols_nodes, inter_nodes, inter_nodes_i, inter_nodes_count);
        cur->value = -1;

        return cur;
    }
}
void huffman_load_codes(HuffmanCoder* coder, FILE* stream){
    assert(coder->intermediate_nodes == NULL);
    assert(coder->symbols_nodes == NULL);

    l_generate_symbols_nodes(coder);
    Node* sub_nodes = calloc(HUFF_ALPHA_LEN, sizeof(Node));
    coder->intermediate_nodes = sub_nodes;
    uchar buffer = 0;
    int bit_index = 8;
    int inter_nodes_i = 0;
    coder->root_node = l_load_code(
            stream,
            &buffer,
            &bit_index,
            coder->symbols_nodes,
            coder->intermediate_nodes,
            &inter_nodes_i,
            HUFF_ALPHA_LEN
            );
    coder->codes = l_get_codes(coder->root_node, HUFF_ALPHA_LEN);
}


void append_bit(int bit_value, FILE* stream, uchar* buffer, int* bit_index){
    //printf("%d ", bit_value);
    if(*bit_index >= 8){
        //fprintf(stderr, "%d ", *buffer);
        fwrite(buffer, sizeof(char), 1, stream);
        *buffer = 0;
        *bit_index = 0;
    }
    *buffer |= bit_value << (*bit_index)++;
}

void l_save_code(Node* node, FILE* stream, uchar* buffer, int* bit_index){
    if(node->value != -1){
        append_bit(1, stream, buffer, bit_index);
        for (int i = 0; i < 8; ++i) {
            append_bit((node->value & (1 << i)) != 0, stream, buffer, bit_index);
        }
    }
    else{
        append_bit(0, stream, buffer, bit_index);
        l_save_code(node->left, stream, buffer, bit_index);
        l_save_code(node->right, stream, buffer, bit_index);
    }
}

// return count of written bytes
int64_t huffman_save_codes(HuffmanCoder* coder, FILE* stream){
    int64_t begin_pos = ftello64(stream);

    uchar buffer = 0;
    int bit_index = 0;
    l_save_code(coder->root_node, stream, &buffer, &bit_index);
    if(bit_index > 0){ // fill to entire byte
        for (int i = bit_index - 1; i < 8; ++i) {
            append_bit(0, stream, &buffer, &bit_index);
        }
    }

    return ftello64(stream) - begin_pos;
}

HuffmanCode huffman_encode_symbol(HuffmanCoder* coder, uchar symbol){
    assert(coder->codes);
    return coder->codes[symbol];
}

// encode max possible bytes from stream to get bits <= out_buffer_len * 8
// out_buffer_len : bytes count
// out_bit_len : bits count
// out_buffer must be clear
void huffman_encode_symbols(
        HuffmanCoder* coder,
        FILE* stream,
        uchar* out_buffer,
        int out_buffer_len,
        int* out_bit_len,
        int* out_processed_bytes,
        int* stop_reason
        ){
    int encoded_bits = 0;
    int buffer_remain = 0;
    int in_buffer_index = 0;
    int processed_bytes = 0;

    while (encoded_bits <= out_buffer_len * 8){
        if(buffer_remain == 0){
            in_buffer_index = 0;
            buffer_remain =
                    (int)fread(
                            coder->_read_buffer,
                            sizeof(char),
                            coder->_read_buffer_size,
                            stream
                            );
            processed_bytes += buffer_remain;
            if(buffer_remain == 0){
                *stop_reason = STOP_REASON_EOF;
                break;
            }
        }

        HuffmanCode code = huffman_encode_symbol(coder, coder->_read_buffer[in_buffer_index++]);

        bool is_stop = false;
        for (int i = 0; i < code.length; ++i) {
            out_buffer[encoded_bits / 8] |= (uchar)((code.code[i] == '1') << (encoded_bits % 8)); // TODO remake
            encoded_bits++;
            if(encoded_bits == out_buffer_len * 8){
                if(i < code.length - 1){ // need at lest one bit more
                    encoded_bits -= i + 1; // do not take into account the already recorded bits
                }
                else{
                    buffer_remain--; // full byte was used
                }
                is_stop = true;
                break;
            }
        }

        if(is_stop){
            break;
        }

        buffer_remain--;
    }

    // move pointer on buffer_remain bytes back
    // because they were not taken into account
    fseek(stream, -buffer_remain, SEEK_CUR);
    processed_bytes -= buffer_remain;

    if(*stop_reason == 0){
        *stop_reason = STOP_REASON_EOB;
    }

    *out_processed_bytes = processed_bytes;
    *out_bit_len = encoded_bits;
}
void huffman_decode_symbols(
        HuffmanCoder* coder,
        uchar* buffer,
        int buffer_bit_len,
        uchar* out_buffer,
        int* out_len
        ){
    Node* current = coder->root_node;
    int bits  = 0;
    int out_buffer_index = 0;
    for (int i = 0; i < buffer_bit_len / 8 + 1; ++i) {

        int to = int_min(8, buffer_bit_len - i * 8);
        for (int j = 0; j < to; ++j) { // loop for each bit in buffer
            bits++;
            if(buffer[i] & (1 << j)){
                current = current->right;
            }
            else{
                current = current->left;
            }

//            current = (Node*)(
//                    (uint64_t)current->right * ((buffer[i] & (1 << j)) != 0) +
//                    (uint64_t)current->left * !(buffer[i] & (1 << j))
//                    );

//            current = (buffer[i] & (1 << j)) ? current->right : current->left;

            assert(current);
            if(current->value != -1){ // symbol founded
                out_buffer[out_buffer_index++] = (uchar)current->value;
                current = coder->root_node;
            }
        }
    }
    if(current != coder->root_node){
        assert(0 && "The code is corrupted");
    }
    *out_len = out_buffer_index;
}



void huffman_free(HuffmanCoder* coder){
    notnull_free(coder->_read_buffer);
    notnull_free(coder->codes);
    notnull_free(coder->intermediate_nodes);
    notnull_free(coder->symbols_nodes);
}