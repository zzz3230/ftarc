#ifndef FTARC_ARCHIVE_STRUCTS_H_
#define FTARC_ARCHIVE_STRUCTS_H_

#include <stdint.h>
#include "stdbool.h"
#include "compression_algorithms/huffman.h"
#include "utilities/collections.h"

enum e_work_stage{
    WORK_NONE = 0,
    WORK_HANDLING,
    WORK_COMPRESSING,
    WORK_DECOMPRESSING
};


typedef struct s_DynListStr DynListStr;

typedef struct s_archive{
    Str file_name;
    DynListStr* included_files;
    int64_t compressing_current;
    int64_t compressing_total;
    int64_t decompressing_current;
    int64_t decompressing_total;
    enum e_work_stage work_stage;
    bool all_work_finished;
    DynListStr* processed_files;
    HuffmanCoder* current_coder;
    FILE* archive_stream;
    uint64_t archive_hash[16];
} Archive;

typedef struct s_archive_file{
    Str file_name;
    int file_id;
    int64_t compressed_file_size;
    int64_t original_file_size;
    time_t add_date;
    uint8_t file_hash[16];
} ArchiveFile;

#endif
