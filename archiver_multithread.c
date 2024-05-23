//
// Created by zzz32 on 5/17/2024.
//

#include "archiver_multithread.h"



// write code length and code itself;
// also compute hash if hash not NULL
// return total written length in bytes
int write_total_huffman_code_(HuffmanCoder* coder, FILE* out_file, MD5Context* hash){
    int64_t code_len_pos = ftello64(out_file);

    short zero = 0;
    //fseeko64(out_file, sizeof(short), SEEK_CUR); // note code len pos
    fwrite(&zero, sizeof(short), 1, out_file);

    short code_len = (short)huffman_save_codes(coder, out_file); // write code
    int64_t code_end_pos = ftello64(out_file);

    fseeko64(out_file, code_len_pos, SEEK_SET); // return to code len pos
    fwrite(&code_len, sizeof(short), 1, out_file); // write code len pos

    if(hash != NULL){
        md5Update(hash, (uint8_t*) &code_len, 2);
        uint8_t buffer[320]; // huffman code always <= 319 bytes length
        ua_assert(code_len <= 320); // just in case lets u_assert it

        int did_read = (int)fread(buffer, sizeof(uint8_t), code_len, out_file);
        uf_assert(code_len == did_read);


        md5Update(hash, buffer, code_len);
    }

    fseeko64(out_file, code_end_pos, SEEK_SET); // return to end of code

    return (int)(code_len + sizeof(short));
}
void read_total_huffman_code_(HuffmanCoder* coder, FILE* from, MD5Context* hash, bool hash_only){
    short code_len;
    fread(&code_len, sizeof(short), 1, from);

    int64_t code_start_pos = ftello64(from);
    if(!hash_only){
        huffman_load_codes(coder, from);
    }
    //int64_t code_end_pos = ftello64(from);

    if(hash != NULL){
        fseeko64(from, code_start_pos, SEEK_SET);

        md5Update(hash, (uint8_t*) &code_len, 2);
        uint8_t buffer[320]; // huffman code always <= 319 bytes length
        ua_assert(code_len <= 320); // just in case lets u_assert it

        int did_read = (int)fread(buffer, sizeof(uint8_t), code_len, from);
        ua_assert(code_len == did_read);

        md5Update(hash, buffer, code_len);

        fseeko64(from, code_start_pos + code_len, SEEK_SET);
    }
}


typedef volatile struct s_arc_com_th {
    uchar** buffers;
    int* processed_bytes; // len == buffers_count
    int* bits_encoded; // len == buffers_count
    int* stop_reason; // len == buffers_count
    pthread_mutex_t* mutexes;
    int buffers_count;
    HuffmanCoder* coder;
    FILE* from;
    FILE* to;
    int buffer_size;
    uint8_t* file_hash;
    bool from_eof;
    int made_blocks_count;
    int written_blocks_count;
    int64_t from_length;
    int64_t total_length;
    MD5Context* hash_ctx;
    Archive* arc;
} ArcComThreadData;

void* com_making_thread(void* _data){
    ArcComThreadData* data = (ArcComThreadData*)_data;
    int cur_buffer = 0;

    int bits_encoded = -1;
    int processed_bytes = 0;

    while (bits_encoded){

        if(data->made_blocks_count - data->written_blocks_count >= 2){
            continue;
        }

        pthread_mutex_lock(&data->mutexes[cur_buffer]);

        uchar* buffer = data->buffers[cur_buffer];

        memset(buffer, 0, data->buffer_size);

        int stop_reason;
        huffman_encode_symbols(
                data->coder,
                data->from,
                buffer,
                data->buffer_size,
                &bits_encoded,
                &processed_bytes,
                &stop_reason
        );

        if(ftello64(data->from) == data->from_length){
            stop_reason = STOP_REASON_EOF;
        }

        //printf("created %d\n", data->made_blocks_count);

        data->made_blocks_count++;
        data->processed_bytes[cur_buffer] = processed_bytes;
        data->bits_encoded[cur_buffer] = bits_encoded;
        data->stop_reason[cur_buffer] = stop_reason;

        pthread_mutex_unlock(&data->mutexes[cur_buffer]);

        cur_buffer++;
        if(cur_buffer >= data->buffers_count){
            cur_buffer = 0;
        }
    }

    data->from_eof = true;

    return NULL;
}

void* com_writing_thread(void* _data){
    ArcComThreadData* data = (ArcComThreadData*)_data;
    int cur_buffer = 0;

    while (1){

        if(!data->from_eof && data->written_blocks_count == data->made_blocks_count){
            //printf("\n\nwaiting\n\n");
            continue; // wait for new blocks or end of com_making_thread
        }

        if(data->from_eof && data->written_blocks_count == data->made_blocks_count){
            break;
        }

        if(any_exceptions())
            break;

        pthread_mutex_lock(&data->mutexes[cur_buffer]);

        //ANY_TIMING_BEGIN

        if(data->processed_bytes[cur_buffer] > 0){
            // write original_size
            short short_proc_bytes = (short)data->processed_bytes[cur_buffer];
            if(fwrite(&short_proc_bytes, sizeof(short), 1, data->to) != 1){
                uf_assert(0);
            }
            // write padding_length
            short padding = (short)((data->buffer_size * 8 - data->bits_encoded[cur_buffer]));     // padding to byte:  aaa00000 00000000 00000000
            //                     ^^^^^ ^^^^^^^^ ^^^^^^^^
            //                     this is padding

            if(fwrite(&padding, sizeof(short), 1, data->to) != 1){
                uf_assert(0);
            }

            int bytes_count = data->bits_encoded[cur_buffer] / 8;
            if(padding % 8 != 0){
                bytes_count++;
            }


            int to_write_length = data->buffer_size;
            if(data->stop_reason[cur_buffer] == STOP_REASON_EOF){
                to_write_length = bytes_count;
                //printf("!!! %d\n", bytes_count);
            }

            md5Update(data->hash_ctx, data->buffers[cur_buffer], bytes_count);

            // write data
            int written = (int)fwrite(data->buffers[cur_buffer], sizeof(char), to_write_length, data->to);
            uf_assert(written == to_write_length);

//            printf("written %d\n", to_write_length);
//            fflush(stdout);

            data->arc->compressing_current += data->processed_bytes[cur_buffer];
            data->total_length += (int)(sizeof(short) + sizeof(short)) + to_write_length;
        }

        data->written_blocks_count++;
        pthread_mutex_unlock(&data->mutexes[cur_buffer]);

        cur_buffer++;
        if(cur_buffer >= data->buffers_count){
            cur_buffer = 0;
        }
    }

    return NULL;
}

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
){
    MD5Context hash_ctx;
    md5Init(&hash_ctx);

    if(any_exceptions())
        return;

    int64_t start_pos = ftello64(to);
    fseeko64(to, sizeof(int64_t) + 16, SEEK_CUR); // skip length and hash

    write_total_huffman_code_(coder, to, &hash_ctx);

    time_t current_time = time(NULL);
    fwrite(&current_time, sizeof(time_t), 1, to);
    md5Update(&hash_ctx, (uint8_t *) &current_time, sizeof(time_t));

    int64_t original_size = file_length(from);
    fwrite(&original_size, sizeof(int64_t), 1, to);
    md5Update(&hash_ctx, (uint8_t *) &original_size, sizeof(int64_t));

    arc->compressing_current = 0;
    arc->compressing_total = original_size;

    short short_file_name_length = (short)from_filename.length;
    fwrite(&short_file_name_length, sizeof(short), 1, to);
    md5Update(&hash_ctx, (uint8_t*) &short_file_name_length, sizeof(short));

    fwrite(from_filename.value, sizeof(char), from_filename.length, to);
    md5Update(&hash_ctx, (uint8_t*) from_filename.value, short_file_name_length);


    int processed_bytes = 0;
    int64_t length = ftello64(to) - start_pos;

    ArcComThreadData th_data = {0};

    pthread_mutex_t mutexes[4] = {0};
    for (int i = 0; i < 4; ++i) {
        pthread_mutex_init(&mutexes[i], NULL);
    }

    th_data.mutexes = mutexes;

    th_data.buffers = buffers;
    th_data.buffers_count = buffets_count;
    th_data.buffer_size = buffer_size;

    th_data.to = to;
    th_data.from = from;

    th_data.hash_ctx = &hash_ctx;
    th_data.coder = coder;
    th_data.arc = arc;

    th_data.processed_bytes = calloc(buffets_count, sizeof(int));
    th_data.bits_encoded = calloc(buffets_count, sizeof(int));
    th_data.stop_reason = calloc(buffets_count, sizeof(int));

    th_data.from_length = file_length(from);

    pthread_t making_t;
    pthread_t writing_t;
    pthread_create(&making_t, NULL, com_making_thread, (void *) &th_data);
    pthread_create(&writing_t, NULL, com_writing_thread, (void *) &th_data);

    pthread_join(making_t, NULL);
    pthread_join(writing_t, NULL);

    free(th_data.processed_bytes);
    free(th_data.bits_encoded);
    free(th_data.stop_reason);

    for (int i = 0; i < 4; ++i) {
        pthread_mutex_destroy(&mutexes[i]);
    }

    length += th_data.total_length;


    int64_t file_end_pos = ftello64(to);
    fseeko64(to, start_pos, SEEK_SET);

    md5Update(&hash_ctx, (uint8_t*) &length, sizeof(int64_t));
    md5Finalize(&hash_ctx);

    fwrite(&length, sizeof(int64_t), 1, to);
    fwrite(hash_ctx.digest, sizeof(uint8_t), 16, to); // save hash
    fseeko64(to, file_end_pos, SEEK_SET);

    if(file_hash != NULL){
        for (int i = 0; i < 16; ++i) {
            file_hash[i] = hash_ctx.digest[i];
        }
    }
}


typedef volatile struct s_arc_ext_th {
    pthread_mutex_t* reading_mutex;
    pthread_mutex_t* writing_mutex;
    pthread_mutex_t* init_mutex;
    pthread_mutex_t* editing_mutex;
    FILE* to;
    FILE* from;
    int64_t total_blocks_count;
    int64_t extracted_blocks;
    int64_t last_writing_offset;
    int64_t first_block_offset;
    int64_t block_length;
    MD5Context* hash_ctx;
    Archive* arc;
    int out_buffer_length;
    uchar** out_buffers;
    int* decoded_bytes;
    int out_buffers_count;
    int64_t* buffers2block;

} ArcExtThreadData;

void* ext_reading_thread(void* _data){
    ArcExtThreadData* data = (ArcExtThreadData*)_data;

    uint64_t full_block_length = sizeof(short) + sizeof(short) + data->block_length;
    uchar* buffer = malloc(full_block_length);
    uf_assert(buffer != NULL);

    pthread_mutex_lock(data->init_mutex);
    int64_t my_buffer_index = data->out_buffers_count++;

    uint64_t out_buffer_size = data->block_length * 8;
    uchar* out_buffer = malloc(out_buffer_size);
    uf_assert(out_buffer != NULL);
    data->out_buffers[my_buffer_index] = out_buffer;

    pthread_mutex_unlock(data->init_mutex);

    while (1){
        while (data->buffers2block[my_buffer_index] != -1){
            if(any_exceptions()) break;
            // waiting for writing
        }

        if(any_exceptions()) break;

        pthread_mutex_lock(data->reading_mutex);
        if(data->total_blocks_count <= data->extracted_blocks){
            // all blocks extracted
            pthread_mutex_unlock(data->reading_mutex);
            break;
        }


        int64_t current_block_index = data->extracted_blocks++;
        uint64_t block_begin_offset = data->first_block_offset + full_block_length * current_block_index;
        uf_assert(block_begin_offset == ftello64(data->from));

        bool is_last_block = current_block_index == data->total_blocks_count - 1;

        short original_size;
        short padding;

        uchar* payload_buffer_ptr = buffer;
        if(!is_last_block){
            fread(buffer, sizeof(uchar), full_block_length, data->from);

            original_size = *((short*)payload_buffer_ptr);
            payload_buffer_ptr += sizeof(short);
            padding = *((short*)payload_buffer_ptr);
            payload_buffer_ptr += sizeof(short);

        }
        else{
            fread(&original_size, sizeof(short), 1, data->from);

            fread(&padding, sizeof(short), 1, data->from);

            fread(buffer, sizeof(char), data->block_length - padding / 8, data->from);
        }

        data->last_writing_offset += original_size;

        md5Update(data->hash_ctx, payload_buffer_ptr, data->block_length - padding / 8);

        pthread_mutex_unlock(data->reading_mutex);

        int decoded_bytes = 0;
        huffman_decode_symbols(
                data->arc->current_coder,
                payload_buffer_ptr,
                (int)(data->block_length * 8 - padding),
                out_buffer,
                &decoded_bytes
                );

        pthread_mutex_lock(data->editing_mutex);
        data->decoded_bytes[my_buffer_index] = decoded_bytes;
        data->buffers2block[my_buffer_index] = current_block_index;
        pthread_mutex_unlock(data->editing_mutex);
    }

    free(out_buffer);
    free(buffer);
}

void* ext_writing_thread(void* _data){
    ArcExtThreadData* data = (ArcExtThreadData*)_data;

    int written_blocks = 0;

    while (1) {
        if(any_exceptions()) break;

        int next_buffer = -1;
        for (int i = 0; i < data->out_buffers_count; ++i) {
            if(data->buffers2block[i] == written_blocks){
                next_buffer = i;
                break;
            }
        }

        if (next_buffer == -1 && written_blocks == data->total_blocks_count) {
            // all blocks extracted
            break;
        }

        if(next_buffer == -1) {
            continue;
        }


        if(!any_exceptions()){
            uchar* out_buffer = data->out_buffers[next_buffer];
            int decoded_bytes = data->decoded_bytes[next_buffer];

            int wrote = (int)fwrite(out_buffer, sizeof(char), decoded_bytes, data->to);
            uf_assert(decoded_bytes == wrote);
            data->arc->decompressing_current++;

            pthread_mutex_lock(data->editing_mutex);
            data->buffers2block[next_buffer] = -1;
            pthread_mutex_unlock(data->editing_mutex);

            written_blocks++;
        }
    }
}


void read_and_dec_file__multithread(
        Archive* arc,
        HuffmanCoder* coder,
        FILE* from,
        FILE* to,
        int buffer_size,
        uint8_t* file_hash
){
    arc->current_coder = coder;

    int64_t file_begin_pos = ftello64(from);

    int64_t length;
    fread(&length, sizeof(int64_t), 1, from);

    uchar saved_hash[16];
    fread(saved_hash, sizeof(uchar), 16, from);

    MD5Context hash_ctx;
    md5Init(&hash_ctx);

    read_total_huffman_code_(coder, from, &hash_ctx, false);

    time_t date;
    fread(&date, sizeof(time_t), 1, from);
    md5Update(&hash_ctx, (uint8_t*)&date, sizeof(date));

    int64_t original_length;
    fread(&original_length, sizeof(int64_t), 1, from);
    md5Update(&hash_ctx, (uint8_t*)&original_length, sizeof(original_length));


    short name_length;
    fread(&name_length, sizeof(short), 1, from);
    ua_assert(name_length > 0);
    md5Update(&hash_ctx, (uint8_t*)&name_length, sizeof(name_length));

    char* name = malloc(name_length);
    uf_assert(name);
    fread(name, sizeof(char), name_length, from);
    md5Update(&hash_ctx, (uint8_t*)name, name_length);
    free(name);

    int64_t blocks_length = length - (ftello64(from) - file_begin_pos);

    int block_size = buffer_size + (int)(sizeof(short) + sizeof(short));

    arc->decompressing_total = blocks_length / block_size + 1;
    arc->decompressing_current = 0;

    int blocks_count = (int)(blocks_length / (int64_t)(block_size) + 1);

    ArcExtThreadData th_data = {0};

    th_data.block_length = buffer_size;
    th_data.from = from;
    th_data.to = to;

    pthread_mutex_t selecting_mutex;
    th_data.reading_mutex = &selecting_mutex;
    pthread_mutex_init(th_data.reading_mutex, NULL);

    pthread_mutex_t init_mutex;
    th_data.init_mutex = &init_mutex;
    pthread_mutex_init(th_data.init_mutex, NULL);

    pthread_mutex_t writing_mutex;
    th_data.writing_mutex = &writing_mutex;
    pthread_mutex_init(th_data.writing_mutex, NULL);

    pthread_mutex_t editing_mutex;
    th_data.editing_mutex = &editing_mutex;
    pthread_mutex_init(th_data.editing_mutex, NULL);

    th_data.arc = arc;
    th_data.total_blocks_count = blocks_count;
    th_data.first_block_offset = ftello64(from);
    th_data.hash_ctx = &hash_ctx;

    const int threads_count = 8;

    th_data.buffers2block = calloc(threads_count, sizeof(int64_t));
    th_data.decoded_bytes = calloc(threads_count, sizeof(int));
    th_data.out_buffers = calloc(threads_count, sizeof(uchar*));

    for (int i = 0; i < threads_count; ++i) {
        th_data.buffers2block[i] = -1;
    }

    pthread_t reading_threads[threads_count];

    for (int i = 0; i < sizeof(reading_threads) / sizeof(reading_threads[0]); ++i) {
        int res = pthread_create(&reading_threads[i], NULL, ext_reading_thread, (void *) &th_data);
        uf_assert(res == 0);
    }

    pthread_t writing_thread;
    pthread_create(&writing_thread, NULL, ext_writing_thread, (void*)&th_data);

    uf_assert(0 == pthread_join(writing_thread, NULL));
    for (int i = 0; i < sizeof(reading_threads) / sizeof(reading_threads[0]); ++i) {
        int res = pthread_join(reading_threads[i], NULL);
        uf_assert(res == 0);
    }

    free(th_data.buffers2block);
    free(th_data.decoded_bytes);
    free(th_data.out_buffers);

    md5Update(&hash_ctx, (uint8_t*)&length, sizeof(length));
    md5Finalize(&hash_ctx);

    if(file_hash != NULL){
        for (int i = 0; i < 16; ++i) {
            file_hash[i] = hash_ctx.digest[i];
        }
    }

    for (int i = 0; i < 16; ++i) {
        if(saved_hash[i] != hash_ctx.digest[i]){
            throw(EXCEPTION_ARCHIVE_IS_CORRUPTED, "file hash didn't match");
        }
    }
}