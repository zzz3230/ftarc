

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "archiver.h"
#include "string.h"
#include "compression_algorithms/huffman.h"

Archive* archive_init(){
    Archive* arc = calloc(1, sizeof(Archive));
    arc->included_files = dl_str_create(16);
    arc->processed_files = dl_str_create(16);
    return arc;
}

Archive* archive_open(Str file_name){
    Archive* arc = archive_init();
    arc->file_name = file_name;
    arc->archive_stream = fopen64(file_name.value, "rb");
    return arc;
}

Archive* archive_new(Str file_name){
    Archive* arc = archive_init();
    arc->file_name = file_name;
    arc->archive_stream = fopen64(file_name.value, "wb");
    return arc;
}

bool archive_validate(){

}

// ptr in stream must be in start of the file
// after function execute ptr won't change his position
// file_id used only for set info.file_id
ArchiveFile get_file_info(FILE* stream, int file_id){
    int64_t prev_pos = ftell(stream);

    ArchiveFile info = {0};
    //fseeko64(stream, sizeof(int64_t) + 16 + sizeof(time_t) + sizeof(int64_t), SEEK_CUR);

    fread(&info.compressed_file_size, sizeof(int64_t), 1, stream);
    fread(&info.file_hash, sizeof(uint8_t), 16, stream);

    short code_len;
    fread(&code_len, sizeof(short), 1, stream);
    fseeko64(stream, code_len, SEEK_CUR);

    fread(&info.add_date, sizeof(time_t), 1, stream);
    fread(&info.original_file_size, sizeof(int64_t), 1, stream);

    short file_name_len;
    fread(&file_name_len, sizeof(short), 1, stream);
    Str name = str_empty(file_name_len);
    fread(name.value, sizeof(char), file_name_len, stream);

    info.file_name = name;
    info.file_id = file_id;

    fseeko64(stream, prev_pos, SEEK_SET);
    return info;
}


void read_total_huffman_code(HuffmanCoder* coder, FILE* from, MD5Context* hash){
    short code_len;
    fread(&code_len, sizeof(short), 1, from);

    int64_t code_start_pos = ftell(from);
    huffman_load_codes(coder, from);
    int64_t code_end_pos = ftell(from);

    if(hash != NULL){
        fseeko64(from, code_start_pos, SEEK_SET);

        md5Update(hash, (uint8_t*) &code_len, 2);
        uint8_t buffer[320]; // huffman code always <= 319 bytes length
        assert(code_len <= 320); // just in case lets assert it

        assert(code_len == fread(buffer, sizeof(uint8_t), code_len, from));

        md5Update(hash, buffer, code_len);

        fseeko64(from, code_end_pos, SEEK_SET);
    }
}

// write code length and code itself;
// also compute hash if hash not NULL
// return total written length in bytes
int write_total_huffman_code(HuffmanCoder* coder, FILE* out_file, MD5Context* hash){
    int64_t code_len_pos = ftell(out_file);
    fseeko64(out_file, sizeof(short), SEEK_CUR); // note code len pos

    short code_len = (short)huffman_save_codes(coder, out_file); // write code
    int64_t code_end_pos = ftell(out_file);

    fseeko64(out_file, code_len_pos, SEEK_SET); // return to code len pos
    fwrite(&code_len, sizeof(short), 1, out_file); // write code len pos

    if(hash != NULL){
        md5Update(hash, (uint8_t*) &code_len, 2);
        uint8_t buffer[320]; // huffman code always <= 319 bytes length
        assert(code_len <= 320); // just in case lets assert it

        assert(code_len == fread(buffer, sizeof(uint8_t), code_len, out_file));

        md5Update(hash, buffer, code_len);
    }

    fseeko64(out_file, code_end_pos, SEEK_SET); // return to end of code

    return (int)(code_len + sizeof(short));
}

void archive_add_file(Archive* arc, Str file_name){
    dl_str_append(arc->included_files, file_name);
}

void handle_file(Archive* arc, HuffmanCoder* coder){

}

// return  written bytes count
int make_and_write_block(
        HuffmanCoder* coder,
        FILE* from,
        FILE* to,
        uchar* buffer,
        int buffer_size,
        int* out_processed_bytes,
        MD5Context* hash_ctx
        ){
    int bits_encoded = 0;
    int processed_bytes = 0;

    memset(buffer, 0, buffer_size);

    int cur = ftell(to);

    int stop_reason;

    huffman_encode_symbols(
            coder,
            from,
            buffer,
            buffer_size,
            &bits_encoded,
            &processed_bytes,
            &stop_reason
            );

    if(bits_encoded > 0){
        // write original_size
        short short_proc_bytes = (short)processed_bytes;
        if(fwrite(&short_proc_bytes, sizeof(short), 1, to) != 1){
            assert(0);
        }
        // write padding_length
        short padding = (short)((buffer_size * 8 - bits_encoded));     // padding to byte:  aaa00000 00000000 00000000
                                                                        //                     ^^^^^ this is padding

        if(fwrite(&padding, sizeof(short), 1, to) != 1){
            assert(0);
        }

        int bytes_count = bits_encoded / 8;
        if(padding != 0){
            bytes_count++;
        }

        //printf("Enc %ld %d\n", ftell(to), bits_encoded);

        int to_write_length = buffer_size;
        if(stop_reason == STOP_REASON_EOF){
            to_write_length = bytes_count;
        }

        md5Update(hash_ctx, buffer, to_write_length);


        // write data
        int written = (int)fwrite(buffer, sizeof(char), to_write_length, to);
        assert(written == to_write_length);


        *out_processed_bytes = processed_bytes;
        return to_write_length + sizeof(short) + sizeof(short);
    }

    return 0;
}

void make_and_write_file(
        Archive* arc,
        HuffmanCoder* coder,
        FILE* from,
        FILE* to,
        Str from_filename,
        uchar* buffer,
        int buffer_size,
        uint8_t* file_hash
        ){
    MD5Context hash_ctx;
    md5Init(&hash_ctx);


    int64_t start_pos = ftell(to);
    fseeko64(to, sizeof(int64_t) + 16, SEEK_CUR);

    write_total_huffman_code(coder, to, &hash_ctx);

    time_t current_time = time(NULL);
    fwrite(&current_time, sizeof(time_t), 1, to);
    md5Update(&hash_ctx, (uint8_t *) &current_time, sizeof(time_t));

    int64_t original_size = file_length(from);
    fwrite(&original_size, sizeof(int64_t), 1, to);
    md5Update(&hash_ctx, (uint8_t *) &original_size, sizeof(int64_t));

    arc->compressing_total = original_size;
    arc->compressing_current = 0;


    short short_file_name_length = (short)from_filename.length;
    fwrite(&short_file_name_length, sizeof(short), 1, to);
    md5Update(&hash_ctx, (uint8_t*) &short_file_name_length, sizeof(short));

    fwrite(from_filename.value, sizeof(char), from_filename.length, to);
    md5Update(&hash_ctx, (uint8_t*) from_filename.value, short_file_name_length);


    int processed_bytes = 0;
    int64_t length = ftell(to) - start_pos;

    int64_t block_length = -1;
    while (block_length){
        block_length = make_and_write_block(coder, from, to, buffer, buffer_size, &processed_bytes, &hash_ctx);

        length += block_length;
        arc->compressing_current += processed_bytes;
    }

    // move to end of last block,
    // because make_and_write_block() fill remaining bytes with zero
//    fseeko64(to, - (buffer_size - block_length), SEEK_CUR);
//    length -= (buffer_size - block_length);


    int64_t file_end_pos = ftell(to);
    fseeko64(to, start_pos, SEEK_SET);

    md5Update(&hash_ctx, (uint8_t*) &length, sizeof(int64_t));
    md5Finalize(&hash_ctx);

    //print_hash(hash_ctx.digest);
    fwrite(&length, sizeof(int64_t), 1, to);
    fwrite(hash_ctx.digest, sizeof(int64_t), 1, to); // save hash
    fseeko64(to, file_end_pos, SEEK_SET);

    if(file_hash != NULL){
        for (int i = 0; i < 16; ++i) {
            file_hash[i] = hash_ctx.digest[i];
        }
    }
    //arc->work_stage = WORK_NONE;
}

void read_and_dec_block(
        HuffmanCoder* coder,
        FILE* from,
        FILE* to,
        uchar* buffer,
        int buffer_size,
        int block_length,
        uchar* out_buffer,
        int out_buffer_size
        ){
    
    short original_size;
    fread(&original_size, sizeof(short), 1, from);

    assert(out_buffer_size >= original_size);
    
    short padding;
    fread(&padding, sizeof(short), 1, from);
    int cur = ftell(from);

    //printf("Dec %ld %d\n", ftell(from), block_length * 8 - padding);

    int read_count = fread(buffer, sizeof(char), buffer_size, from);
    int decoded_bytes = 0;


    huffman_decode_symbols(coder, buffer, block_length * 8 - padding, out_buffer, &decoded_bytes);

    assert(decoded_bytes == fwrite(out_buffer, sizeof(char), decoded_bytes, to));
}

void read_and_dec_file(
        Archive* arc,
        HuffmanCoder* coder,
        FILE* from,
        FILE* to,
        uchar* buffer,
        int buffer_size,
        uchar* out_buffer,
        int out_buffer_size
        ){

    int64_t file_begin_pos = ftell(from);

    int64_t length;
    fread(&length, sizeof(int64_t), 1, from);

    uchar hash[16];
    fread(hash, sizeof(uchar), 16, from);

    MD5Context hash_ctx;
    md5Init(&hash_ctx);

    read_total_huffman_code(coder, from, &hash_ctx);

    fseeko64(from, sizeof(time_t), SEEK_CUR); // skip date
    fseeko64(from, sizeof(int64_t), SEEK_CUR); // skip original_length

    short name_length;
    fread(&name_length, sizeof(short), 1, from);
    fseeko64(from, name_length, SEEK_CUR);

    length -= ftell(from) - file_begin_pos;

    arc->decompressing_total = length / buffer_size + 1;
    arc->decompressing_current = 0;

    for (int i = 0; i < length / buffer_size + 1; ++i) {
        int64_t block_length = int64_min(buffer_size, length - buffer_size * i);
        read_and_dec_block(
                coder,
                from,
                to,
                buffer,
                buffer_size,
                buffer_size,
                out_buffer,
                out_buffer_size
                );

        arc->decompressing_current++;
        //printf("%lld\n", arc->decompressing_current);
    }

    //arc->work_stage = WORK_NONE;
}


void archive_compress(Archive* arc){
    HuffmanCoder* coder = huffman_coder_create();

//    for (int i = 0; i < arc->included_files->count; ++i) {
//        FILE* file = fopen(dl_str_get(arc->included_files, i).value, "rb");
//        huffman_handle_file(coder, file);
//        fclose(file);
//    }
//    huffman_build_codes(coder);

    FILE* out_file = arc->archive_stream;

    fwrite(ARCHIVE_FILE_BEGIN, sizeof(char), sizeof(ARCHIVE_FILE_BEGIN) - 1, out_file);

    int64_t hash_pos = ftell(out_file);
    fseeko64(out_file, 16, SEEK_CUR);

    int files_count = arc->included_files->count;
    fwrite(&files_count, sizeof(int), 1, out_file);


    MD5Context archive_hash;
    md5Init(&archive_hash);

    uint8_t file_hash[16];

    uchar buffer[BUFFER_LENGTH] = {0};

    arc->work_stage = WORK_COMPRESSING;

    for (int i = 0; i < arc->included_files->count; ++i) {
        Str file_to_compress = dl_str_get(arc->included_files, i);
        FILE *file = fopen64(file_to_compress.value, "rb");
        dl_str_append(arc->processed_files, file_to_compress);

        // HANDLING FILE
        huffman_clear(coder);
        huffman_handle_file(coder, file);
        huffman_build_codes(coder);

        Str in_archive_file_name = str(get_file_name_from_path(file_to_compress.value));

        // COMPRESSING & WRITING FILE
        make_and_write_file(
                arc,
                coder,
                file,
                out_file,
                in_archive_file_name,
                buffer,
                BUFFER_LENGTH,
                file_hash
                );
        fclose(file);

        md5Update(&archive_hash, file_hash, 16);
    }

    md5Finalize(&archive_hash);
    fseeko64(out_file, hash_pos, SEEK_SET);
    fwrite(archive_hash.digest, sizeof(uint8_t), 16, out_file);

    fclose(out_file);

    arc->work_stage = WORK_NONE;

    huffman_free(coder);
}

void archive_decompress(Archive* arc, Str out_path, DynListInt* files_ids){
    HuffmanCoder* coder = huffman_coder_create();
    FILE* compressed = arc->archive_stream;

    char file_sign[sizeof(ARCHIVE_FILE_BEGIN)] = {0};
    fread(file_sign, sizeof(char), sizeof(ARCHIVE_FILE_BEGIN) - 1, compressed);
    assert(strcmp(file_sign, ARCHIVE_FILE_BEGIN));

    uint8_t saved_hash[16];
    fread(saved_hash, sizeof(uint8_t), 16, compressed);

    int files_count;
    fread(&files_count, sizeof(int), 1, compressed);

    uchar buffer[BUFFER_LENGTH] = {0};
    uchar out_buffer[BUFFER_LENGTH*8] = {0};

    for (int i = 0; i < files_count; ++i) {
        int64_t length;
        fread(&length, sizeof(int64_t), 1, compressed);

        if(dl_int_contains(files_ids, i)){
            fseeko64(compressed, -(int)(sizeof(int64_t)), SEEK_CUR); // move ptr because length did read
            ArchiveFile info = get_file_info(compressed, i);

            FILE* decompressed = fopen64(info.file_name.value, "wb");

            huffman_clear(coder);
            read_and_dec_file(
                    arc,
                    coder,
                    compressed,
                    decompressed,
                    buffer,
                    BUFFER_LENGTH,
                    out_buffer,
                    BUFFER_LENGTH*8
                    );

            fclose(decompressed);
        }
        else{
            fseeko64(compressed, length - (int)sizeof(length), SEEK_CUR); // because length include length length :)
        }
    }
}

// also update archive_hash field
DynListArchiveFile* archive_get_files(Archive* arc){
    FILE* compressed = arc->archive_stream;

    char file_sign[sizeof(ARCHIVE_FILE_BEGIN)] = {0};
    fread(file_sign, sizeof(char), sizeof(ARCHIVE_FILE_BEGIN) - 1, compressed);
    assert(strcmp(file_sign, ARCHIVE_FILE_BEGIN) == 0);

    uint8_t saved_hash[16];
    fread(saved_hash, sizeof(uint8_t), 16, compressed);

    for (int i = 0; i < 16; ++i) {
        arc->archive_hash[i] = saved_hash[i];
    }

    int files_count;
    fread(&files_count, sizeof(int), 1, compressed);

    DynListArchiveFile* infos = dl_arc_file_create(files_count);

    for (int i = 0; i < files_count; ++i) {
        ArchiveFile info = get_file_info(compressed, i);
        dl_arc_file_append(infos, info);
        fseeko64(compressed, info.compressed_file_size, SEEK_CUR);
    }

    return infos;
}
void archive_free(Archive* arc){

}
