#ifndef HUFFMAN_H_
#define HUFFMAN_H_

#include <stdint.h>
#include "../utilities/heap.h"
#include "../utilities/utils.h"
#include "../utilities/timings.h"


#define HUFF_ALPHA_LEN 256
#define HANDLE_FILE_BUFFER_SIZE 4096
#define HUFF_READ_FILE_ELEMENT int64_t
#define HUFF_CODE_MAX_LENGTH 32


//EndOfFile
#define STOP_REASON_EOF 1
//EndOfBuffer
#define STOP_REASON_EOB 2

typedef struct s_huffman_code{
    uint64_t length;
    uint64_t code;
} HuffmanCode;

typedef struct s_huffman_coder{
    int64_t counts_table[HUFF_ALPHA_LEN];
    uchar* _read_buffer;
    int _read_buffer_size;
    double read_progress;
    Heap* build_heap;
    Node* symbols_nodes;
    Node* intermediate_nodes;
    HuffmanCode* codes;
    Node* root_node;
} HuffmanCoder;



HuffmanCoder* huffman_coder_create();
void huffman_handle_file(HuffmanCoder* coder, FILE* file);
void huffman_build_codes(HuffmanCoder* coder);
HuffmanCode huffman_encode_symbol(HuffmanCoder* coder, uchar symbol);
void huffman_decode_symbols(HuffmanCoder* coder, uchar* buffer, int buffer_bit_len, uchar* out_buffer, int* out_len);
void huffman_free(HuffmanCoder* coder);
void huffman_encode_symbols(
        HuffmanCoder* coder,
        FILE* stream,
        uchar* out_buffer,
        int out_buffer_len,
        int* out_bit_len,
        int* out_processed_bytes,
        int* stop_reason
);
void huffman_load_codes(HuffmanCoder* coder, FILE* stream);
int64_t huffman_save_codes(HuffmanCoder* coder, FILE* stream);
void huffman_clear(HuffmanCoder* coder);
#endif