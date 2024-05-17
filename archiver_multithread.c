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

void* making_thread(void* _data){
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

void* writing_thread(void* _data){
    ArcComThreadData* data = (ArcComThreadData*)_data;
    int cur_buffer = 0;

    while (1){

        if(!data->from_eof && data->written_blocks_count == data->made_blocks_count){
            //printf("\n\nwaiting\n\n");
            continue; // wait for new blocks or end of making_thread
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
    pthread_create(&making_t, NULL, making_thread, (void*)&th_data);
    pthread_create(&writing_t, NULL, writing_thread, (void*)&th_data);

    pthread_join(making_t, NULL);
    pthread_join(writing_t, NULL);

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
