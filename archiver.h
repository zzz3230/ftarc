#ifndef ARC_H_
#define ARC_H_

#include <stdint.h>
#include "utilities/utils.h"
#include "compression_algorithms/huffman.h"
#include "stdbool.h"
#include "utilities/md5.h"
#include "utilities/collections.h"

#define ARCHIVE_FILE_BEGIN "\002ftarc"
#define BUFFER_LENGTH 4096

Archive* archive_open(Str file_name);
Archive* archive_new(Str file_name);
void archive_add_file(Archive* arc, Str file_name);
void archive_compress(Archive* arc);
void archive_decompress(Archive* arc, Str out_path, DynListInt* files_ids);
DynListArchiveFile* archive_get_files(Archive* arc);
void archive_free(Archive* arc);
void make_and_write_file(
        Archive* arc,
        HuffmanCoder* coder,
        FILE* from,
        FILE* to,
        Str from_filename,
        uchar* buffer,
        int buffer_size,
        uint8_t* file_hash
);
void read_and_dec_file(
        Archive* arc,
        HuffmanCoder* coder,
        FILE* from,
        FILE* to,
        uchar* buffer,
        int buffer_size,
        uchar* out_buffer,
        int out_buffer_size
);
ArchiveFile get_file_info(FILE* stream, int file_id);

#endif