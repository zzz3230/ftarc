

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "archiver.h"
#include "string.h"
#include "compression_algorithms/huffman.h"
#include "exceptions.h"

// validate and skip sign
bool validate_signature(FILE* file){
    char sign[sizeof(ARCHIVE_FILE_BEGIN)] = {0};
    fread(sign, sizeof(char), sizeof(ARCHIVE_FILE_BEGIN) - 1, file);
    return strcmp(sign, ARCHIVE_FILE_BEGIN) == 0;
}

Archive* archive_init(){
    Archive* arc = calloc(1, sizeof(Archive));
    arc->included_files = dl_str_create(16);
    arc->processed_files = dl_str_create(16);
    return arc;
}

Archive* archive_open(Str file_name, bool write_mode){
    Archive* arc = archive_init();
    arc->file_name = file_name;
    arc->archive_stream = fopen64(file_name.value, write_mode ? "wb" : "rb");
    if(arc->archive_stream == NULL){
        throw(EXCEPTION_UNABLE_TO_OPEN_FILE, file_name.value);
    }
//    if(validate_signature(arc->archive_stream) == false){
//        throw(EXCEPTION_NOT_AN_ARCHIVE, file_name.value);
//    }

    return arc;
}

Archive* archive_new(Str file_name){
    Archive* arc = archive_init();
    arc->file_name = file_name;

    if(is_file_exists(file_name.value)){
        throw(EXCEPTION_FILE_ALREADY_EXISTS, file_name.value);
    }

    arc->archive_stream = fopen64(file_name.value, "wb");
    if(arc->archive_stream == NULL){
        throw(EXCEPTION_UNABLE_TO_CREATE_FILE, file_name.value);
    }
    return arc;
}

// ptr must be at the start of file
// ptr will be at the end of header
void read_archive_header(Archive* arc){
    if(validate_signature(arc->archive_stream) == false){
        throw(EXCEPTION_NOT_AN_ARCHIVE, arc->file_name.value);
    }

    u_assert(16 == fread(arc->archive_hash, sizeof(uint8_t), 16, arc->archive_stream));
    u_assert(1 == fread(&arc->archive_files_count, sizeof(int), 1, arc->archive_stream));
}

// ptr must be at the start of file
// ptr will be at the end of header
void update_archive_header(Archive* arc){
    DynListArchiveFile* files = archive_get_files(arc);

    MD5Context arc_hash;
    md5Init(&arc_hash);

    md5Update(&arc_hash, (uint8_t*)&files->count, sizeof(files->count));

    for (int i = 0; i < files->count; ++i) {
        md5Update(&arc_hash, dl_arc_file_get(files, i).file_hash, 16);
    }

    md5Finalize(&arc_hash);


    fseeko64(arc->archive_stream, sizeof(ARCHIVE_FILE_BEGIN) - 1, SEEK_CUR);

    fwrite(arc_hash.digest, sizeof(uint8_t), 16, arc->archive_stream);
    fwrite(&files->count, sizeof(int), 1, arc->archive_stream);
}



// ptr in stream must be in start of the file
// after function execute ptr won't change his position
// file_id used only for set info.file_id
ArchiveFile get_file_info(FILE* stream, int file_id){
    int64_t prev_pos = ftello64(stream);

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


void read_total_huffman_code(HuffmanCoder* coder, FILE* from, MD5Context* hash, bool hash_only){
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
        u_assert(code_len <= 320); // just in case lets u_assert it

        int did_read = (int)fread(buffer, sizeof(uint8_t), code_len, from);
        u_assert(code_len == did_read);

        md5Update(hash, buffer, code_len);

        fseeko64(from, code_start_pos + code_len, SEEK_SET);
    }
}

// write code length and code itself;
// also compute hash if hash not NULL
// return total written length in bytes
int write_total_huffman_code(HuffmanCoder* coder, FILE* out_file, MD5Context* hash){
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
        u_assert(code_len <= 320); // just in case lets u_assert it

        int did_read = (int)fread(buffer, sizeof(uint8_t), code_len, out_file);
        u_assert(code_len == did_read);


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

    //int cur = ftello64(to);

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
            u_assert(0);
        }
        // write padding_length
        short padding = (short)((buffer_size * 8 - bits_encoded));     // padding to byte:  aaa00000 00000000 00000000
                                                                        //                     ^^^^^ ^^^^^^^^ ^^^^^^^^
                                                                        //                     this is padding

        if(fwrite(&padding, sizeof(short), 1, to) != 1){
            u_assert(0);
        }

        int bytes_count = bits_encoded / 8;
        if(padding % 8 != 0){
            bytes_count++;
        }

        //printf("Enc %ld %d\n", ftello64(to), bits_encoded);

        int to_write_length = buffer_size;
        if(stop_reason == STOP_REASON_EOF){
            to_write_length = bytes_count;
            //printf("EOF\n");
        }

        //printf("%d ", bytes_count);
        md5Update(hash_ctx, buffer, bytes_count);

        // write data
        int written = (int)fwrite(buffer, sizeof(char), to_write_length, to);
        u_assert(written == to_write_length);


        *out_processed_bytes = processed_bytes;
        return to_write_length + (int)(sizeof(short) + sizeof(short));
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


    int64_t start_pos = ftello64(to);
    fseeko64(to, sizeof(int64_t) + 16, SEEK_CUR); // skip length and hash

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
    int64_t length = ftello64(to) - start_pos;



    int blocks_count = 0;

    int64_t block_length = -1;
    while (block_length){
        block_length = make_and_write_block(coder, from, to, buffer, buffer_size, &processed_bytes, &hash_ctx);
        blocks_count++;
        length += block_length;
        arc->compressing_current += processed_bytes;
    }


    //printf("[%lld]", length);

    // move to end of last block,
    // because make_and_write_block() fill remaining bytes with zero
//    fseeko64(to, - (buffer_size - block_length), SEEK_CUR);
//    length -= (buffer_size - block_length);


    int64_t file_end_pos = ftello64(to);
    fseeko64(to, start_pos, SEEK_SET);

    md5Update(&hash_ctx, (uint8_t*) &length, sizeof(int64_t));
    md5Finalize(&hash_ctx);

    //print_hash(hash_ctx.digest);
    fwrite(&length, sizeof(int64_t), 1, to);
    fwrite(hash_ctx.digest, sizeof(uint8_t), 16, to); // save hash
    fseeko64(to, file_end_pos, SEEK_SET);

    if(file_hash != NULL){
        for (int i = 0; i < 16; ++i) {
            file_hash[i] = hash_ctx.digest[i];
        }
    }
    //arc->work_stage = WORK_NONE;
}


// if hash_only == true
//      func compute only hash AND coder, to, out_buffer can be NULL
void read_and_dec_block(
        HuffmanCoder* coder,
        FILE* from,
        FILE* to,
        uchar* buffer,
        int buffer_size,
        int block_length,
        uchar* out_buffer,
        int out_buffer_size,
        MD5Context* hash_ctx,
        bool is_last_block,
        bool hash_only
        ){
    
    short original_size;
    fread(&original_size, sizeof(short), 1, from);

    u_assert(out_buffer_size >= original_size);
    
    short padding;
    fread(&padding, sizeof(short), 1, from);

    int count_to_read = is_last_block ? block_length - padding / 8 : block_length;

    fread(buffer, sizeof(char), count_to_read, from);
    int decoded_bytes = 0;

    md5Update(hash_ctx, buffer, block_length - padding / 8);

    if(!hash_only){
        huffman_decode_symbols(coder, buffer, block_length * 8 - padding, out_buffer, &decoded_bytes);

        int wrote = (int)fwrite(out_buffer, sizeof(char), decoded_bytes, to);
        u_assert(decoded_bytes == wrote);
    }
}

// if hash_only == true
//      func compute only hash AND coder, to, out_buffer can be NULL
void read_and_dec_file(
        Archive* arc,
        HuffmanCoder* coder,
        FILE* from,
        FILE* to,
        uchar* buffer,
        int buffer_size,
        uchar* out_buffer,
        int out_buffer_size,
        uint8_t* file_hash,
        bool hash_only
        ){

    int64_t file_begin_pos = ftello64(from);

    int64_t length;
    fread(&length, sizeof(int64_t), 1, from);

    uchar saved_hash[16];
    fread(saved_hash, sizeof(uchar), 16, from);

    MD5Context hash_ctx;
    md5Init(&hash_ctx);

    read_total_huffman_code(coder, from, &hash_ctx, hash_only);

    time_t date;
    fread(&date, sizeof(time_t), 1, from);
    md5Update(&hash_ctx, (uint8_t*)&date, sizeof(date));

    int64_t original_length;
    fread(&original_length, sizeof(int64_t), 1, from);
    md5Update(&hash_ctx, (uint8_t*)&original_length, sizeof(original_length));


    short name_length;
    fread(&name_length, sizeof(short), 1, from);
    md5Update(&hash_ctx, (uint8_t*)&name_length, sizeof(name_length));

    char* name = malloc(name_length);
    u_assert(name);
    fread(name, sizeof(char), name_length, from);
    md5Update(&hash_ctx, (uint8_t*)name, name_length);
    free(name);

    int64_t blocks_length = length - (ftello64(from) - file_begin_pos);

    int block_size = buffer_size + (int)(sizeof(short) + sizeof(short));

    arc->decompressing_total = blocks_length / block_size + 1;
    arc->decompressing_current = 0;

    int blocks_count =(int)(blocks_length / (int64_t)(block_size) + 1);
    for (int i = 0; i < blocks_count; ++i) {
        read_and_dec_block(
                coder,
                from,
                to,
                buffer,
                buffer_size,
                buffer_size,
                out_buffer,
                out_buffer_size,
                &hash_ctx,
                i == blocks_count - 1,
                hash_only
                );

        arc->decompressing_current++;
    }

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


void archive_save(Archive* arc){
    HuffmanCoder* coder = huffman_coder_create();
    arc->current_coder = coder;


    FILE* out_file = arc->archive_stream;

    fwrite(ARCHIVE_FILE_BEGIN, sizeof(char), sizeof(ARCHIVE_FILE_BEGIN) - 1, out_file);

    int64_t hash_pos = ftello64(out_file);
    fseeko64(out_file, 16, SEEK_CUR);

    int files_count = arc->included_files->count;
    fwrite(&files_count, sizeof(int), 1, out_file);


    MD5Context archive_hash;
    md5Init(&archive_hash);

    md5Update(&archive_hash, (uint8_t*) &arc->included_files->count, sizeof(int));

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

// extract files with given ids and names
// if some id and name point to same file extract will once
void archive_extract(Archive* arc, Str out_path, DynListInt* files_ids){
    HuffmanCoder* coder = huffman_coder_create();
    FILE* compressed = arc->archive_stream;

    char file_sign[sizeof(ARCHIVE_FILE_BEGIN)] = {0};
    fread(file_sign, sizeof(char), sizeof(ARCHIVE_FILE_BEGIN) - 1, compressed);
    u_assert(strcmp(file_sign, ARCHIVE_FILE_BEGIN) == 0);

    uint8_t saved_hash[16];
    fread(saved_hash, sizeof(uint8_t), 16, compressed);

    int files_count;
    fread(&files_count, sizeof(int), 1, compressed);

    uchar buffer[BUFFER_LENGTH] = {0};
    uchar out_buffer[BUFFER_LENGTH*8] = {0};

    arc->work_stage = WORK_DECOMPRESSING;

    for (int i = 0; i < files_count; ++i) {
        int64_t length;
        fread(&length, sizeof(int64_t), 1, compressed);

        if(dl_int_contains(files_ids, i)){
            fseeko64(compressed, -(int)(sizeof(int64_t)), SEEK_CUR); // move ptr because length did read
            ArchiveFile info = get_file_info(compressed, i);

            //printf("[BEGIN %lld]", ftello64(compressed));

            Str full_path = str_concat_path(out_path, info.file_name);

            FILE* decompressed = fopen64(full_path.value, "wb");

            //printf("%s\n", full_path.value);

            dl_str_append(arc->processed_files, full_path);


            huffman_clear(coder);
            read_and_dec_file(
                    arc,
                    coder,
                    compressed,
                    decompressed,
                    buffer,
                    BUFFER_LENGTH,
                    out_buffer,
                    BUFFER_LENGTH*8,
                    NULL,
                    false
                    );

            fclose(decompressed);

            //printf("[END %lld]", ftello64(compressed));

        }
        else{
            fseeko64(compressed, length - (int)sizeof(length), SEEK_CUR); // because length include length length :)
        }
    }

    arc->work_stage = WORK_NONE;
}

// also update archive_hash field
// rewind FILE on end
DynListArchiveFile* archive_get_files(Archive* arc){
    FILE* compressed = arc->archive_stream;

    char file_sign[sizeof(ARCHIVE_FILE_BEGIN)] = {0};
    fread(file_sign, sizeof(char), sizeof(ARCHIVE_FILE_BEGIN) - 1, compressed);
    u_assert(strcmp(file_sign, ARCHIVE_FILE_BEGIN) == 0);

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

    rewind(compressed);

    return infos;
}

bool archive_validate(Archive* arc){

    arc->work_stage = WORK_VALIDATING;

    read_archive_header(arc);

    uchar buffer[BUFFER_LENGTH] = {0};

    MD5Context current_hash;
    md5Init(&current_hash);

    md5Update(&current_hash, (uint8_t*) arc->archive_hash, sizeof(arc->archive_hash));
    md5Update(&current_hash, (uint8_t*) &arc->archive_files_count, sizeof(int));

    uint8_t file_hash[16];

    for (int i = 0; i < arc->archive_files_count; ++i) {
        read_and_dec_file(
                arc,
                NULL,
                arc->archive_stream,
                NULL,
                buffer,
                BUFFER_LENGTH,
                NULL,
                BUFFER_LENGTH*8,
                file_hash,
                true
        );

        md5Update(&current_hash, file_hash, sizeof(file_hash));
    }

    md5Finalize(&current_hash);

    arc->work_stage = WORK_NONE;

    for (int i = 0; i < 16; ++i) {
        if(current_hash.digest[i] != arc->archive_hash[i]){
            return false;
        }
    }
    return true;
}

void archive_free(Archive* arc){

}
