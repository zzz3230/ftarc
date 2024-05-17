#ifndef FTARC_ARCHIVER_MULTITHREAD_H
#define FTARC_ARCHIVER_MULTITHREAD_H

#include "utilities/md5.h"
#include "utilities/utils.h"
#include "compression_algorithms/huffman.h"
#include "utilities/timings.h"
#include "exceptions.h"
#include "time.h"
#include <pthread.h>

void make_and_write_file__multithread(
        Archive* arc,
        HuffmanCoder* coder,
        FILE* from,
        FILE* to,
        Str from_filename,
        uchar** buffers,
        int buffer_size,
        int buffets_count,
        uint8_t* file_hash
);

#endif //FTARC_ARCHIVER_MULTITHREAD_H