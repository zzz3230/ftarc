

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

// ptr must be at the start of file
// ptr will be at the end of header
void read_archive_header(Archive* arc){
    if(validate_signature(arc->archive_stream) == false){
        throw(EXCEPTION_NOT_AN_ARCHIVE, arc->file_name.value);
    }

    ua_assert(16 == fread(arc->archive_hash, sizeof(uint8_t), 16, arc->archive_stream));
    ua_assert(1 == fread(&arc->archive_files_count, sizeof(int), 1, arc->archive_stream));
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
    arc->archive_stream = fopen64(file_name.value, write_mode ? "rb+" : "rb");
    if(arc->archive_stream == NULL){
        throw(EXCEPTION_UNABLE_TO_OPEN_FILE, file_name.value);
    }

    set_loaded_archive(arc);

    read_archive_header(arc);
    rewind(arc->archive_stream);

//    if(validate_signature(arc->archive_stream) == false){
//        throw(EXCEPTION_NOT_AN_ARCHIVE, file_name.value);
//    }

    return arc;
}

Archive* archive_new(Str file_name, bool override){
    Archive* arc = archive_init();
    arc->file_name = file_name;


    if(!override && is_file_exists(file_name.value)){
        throw(EXCEPTION_FILE_ALREADY_EXISTS, file_name.value);
    }

    arc->archive_stream = fopen64(file_name.value, "wb");
    if(arc->archive_stream == NULL){
        throw(EXCEPTION_UNABLE_TO_CREATE_FILE, file_name.value);
    }

    set_loaded_archive(arc);
    return arc;
}

DynListInt* get_files_ids_by_names(Archive* arc, DynListStr* files){
    DynListInt* final_ids = dl_int_create(8);
    DynListArchiveFile* arc_files = archive_get_files(arc, -1);
    for (int i = 0; i < arc_files->count; ++i) {
        int index = dl_str_find(files, dl_arc_file_get(arc_files, i).file_name);
        if(index != -1){
            dl_int_append(final_ids, i);
            dl_str_remove_at(files, index);
        }
        archive_file_free(dl_arc_file_get_ref(arc_files, i));
    }

    if(files->count > 0){
        throw(EXCEPTION_NOT_FOUND_IN_ARCHIVE, dl_str_get(files, 0).value);
    }

    return final_ids;
}

// ptr must be at the start of file
// ptr will be at the end of header
// max files count getting form arc->archive_files_count
void update_archive_header(Archive* arc){
    //printf("\n\n{%d}\n\n", arc->archive_files_count);
    DynListArchiveFile* files = archive_get_files(arc, arc->archive_files_count);

    MD5Context arc_hash;
    md5Init(&arc_hash);

    md5Update(&arc_hash, (uint8_t*)&files->count, sizeof(files->count));

    for (int i = 0; i < files->count; ++i) {
        md5Update(&arc_hash, dl_arc_file_get(files, i).file_hash, 16);
        archive_file_free(dl_arc_file_get_ref(files, i));
    }

    md5Finalize(&arc_hash);


    fseeko64(arc->archive_stream, sizeof(ARCHIVE_FILE_BEGIN) - 1, SEEK_CUR);

    fwrite(arc_hash.digest, sizeof(uint8_t), 16, arc->archive_stream);
    fwrite(&files->count, sizeof(int), 1, arc->archive_stream);
}


void skip_file(FILE* stream){
    int64_t length;
    fread(&length, sizeof(int64_t), 1, stream);
    fseeko64(stream, length - (int)sizeof(length), SEEK_CUR);
}


// ptr in stream must be in start of the file
// after function execute ptr won't change his position
// file_id used only for set info.file_id
ArchiveFile get_file_info(FILE* stream, int file_id){
    int64_t prev_pos = ftello64(stream);

    ArchiveFile info = {0};
    //fseeko64(stream, sizeof(int64_t) + 16 + sizeof(time_t) + sizeof(int64_t), SEEK_CUR);

    ua_assert(1 == fread(&info.compressed_file_size, sizeof(int64_t), 1, stream));
    ua_assert(16 == fread(&info.file_hash, sizeof(uint8_t), 16, stream));

    short code_len;
    fread(&code_len, sizeof(short), 1, stream);

    ua_assert(code_len >= 0 && code_len <= 320);

    fseeko64(stream, code_len, SEEK_CUR);

    ua_assert(1 == fread(&info.add_date, sizeof(time_t), 1, stream));
    ua_assert(1 == fread(&info.original_file_size, sizeof(int64_t), 1, stream));

    short file_name_len;

    ua_assert(1 == fread(&file_name_len, sizeof(short), 1, stream));
    ua_assert(file_name_len > 0);

    Str name = str_empty(file_name_len);
    ua_assert(file_name_len == fread(name.value, sizeof(char), file_name_len, stream));

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
        ua_assert(code_len <= 320); // just in case lets u_assert it

        int did_read = (int)fread(buffer, sizeof(uint8_t), code_len, from);
        ua_assert(code_len == did_read);

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
        ua_assert(code_len <= 320); // just in case lets u_assert it

        int did_read = (int)fread(buffer, sizeof(uint8_t), code_len, out_file);
        uf_assert(code_len == did_read);


        md5Update(hash, buffer, code_len);
    }

    fseeko64(out_file, code_end_pos, SEEK_SET); // return to end of code

    return (int)(code_len + sizeof(short));
}

void archive_add_file(Archive* arc, Str file_name){
    dl_str_append(arc->included_files, file_name);
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

ANY_TIMING_BEGIN

    int bits_encoded = 0;
    int processed_bytes = 0;

    memset(buffer, 0, buffer_size);

    int64_t from_length = file_length(from);

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

PROCESSOR_TIMING_END

    if(bits_encoded > 0){
        if(any_exceptions())
            return 0;

ANY_TIMING_BEGIN

        // write original_size
        short short_proc_bytes = (short)processed_bytes;
        if(fwrite(&short_proc_bytes, sizeof(short), 1, to) != 1){
            uf_assert(0);
        }
        // write padding_length
        short padding = (short)((buffer_size * 8 - bits_encoded));     // padding to byte:  aaa00000 00000000 00000000
                                                                        //                     ^^^^^ ^^^^^^^^ ^^^^^^^^
                                                                        //                     this is padding

        if(fwrite(&padding, sizeof(short), 1, to) != 1){
            uf_assert(0);
        }

        int bytes_count = bits_encoded / 8;
        if(padding % 8 != 0){
            bytes_count++;
        }

        //printf("Enc %ld %d\n", ftello64(to), bits_encoded);

        int to_write_length = buffer_size;
        if(stop_reason == STOP_REASON_EOF || ftello64(from) == from_length){
            to_write_length = bytes_count;
            //printf("EOF\n");
        }

        //printf("%d ", bytes_count);
        //if(stop_reason != EOF)
        md5Update(hash_ctx, buffer, bytes_count);

//        FILE* f = fopen64("log.txt", "rt+");
//        fprintf(f, "%d\n", bytes_count);
//        fclose(f);

        // write data
        int written = (int)fwrite(buffer, sizeof(char), to_write_length, to);
        uf_assert(written == to_write_length);

FILESYSTEM_TIMING_END

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

    if(any_exceptions())
        return;

    int64_t start_pos = ftello64(to);
    fseeko64(to, sizeof(int64_t) + 16, SEEK_CUR); // skip length and hash

    write_total_huffman_code(coder, to, &hash_ctx);

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



    int blocks_count = 0;

    int64_t block_length = -1;
    while (block_length){
        block_length = make_and_write_block(coder, from, to, buffer, buffer_size, &processed_bytes, &hash_ctx);
        blocks_count++;
        length += block_length;
        arc->compressing_current += processed_bytes;
    }


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

    ANY_TIMING_BEGIN

    short original_size;
    fread(&original_size, sizeof(short), 1, from);

    ua_assert(out_buffer_size >= original_size);
    
    short padding;
    fread(&padding, sizeof(short), 1, from);

    int count_to_read = is_last_block ? block_length - padding / 8 : block_length;

    fread(buffer, sizeof(char), count_to_read, from);
    int decoded_bytes = 0;

    FILESYSTEM_TIMING_END
    ANY_TIMING_BEGIN

    //if(!is_last_block)
    md5Update(hash_ctx, buffer, block_length - padding / 8);

    if(!hash_only){
        huffman_decode_symbols(coder, buffer, block_length * 8 - padding, out_buffer, &decoded_bytes);

        if(!any_exceptions()){
            int wrote = (int)fwrite(out_buffer, sizeof(char), decoded_bytes, to);
            uf_assert(decoded_bytes == wrote);
        }
    }
    PROCESSOR_TIMING_END
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

    arc->work_stage = WORK_COMPRESSING;

    for (int i = 0; i < arc->included_files->count; ++i) {
        char* path = dl_str_get(arc->included_files, i).value;
        if(!is_file_exists(path)){
            throw(EXCEPTION_UNABLE_TO_OPEN_FILE, path);
        }
        else if(!can_read_file(path)){
            throw(EXCEPTION_CANT_READ_FILE, path);
        }
    }

    FILE* out_file = arc->archive_stream;
    arc->writing_file_stream = arc->archive_stream;
    arc->writing_file_name = arc->file_name;

    fwrite(ARCHIVE_FILE_BEGIN, sizeof(char), sizeof(ARCHIVE_FILE_BEGIN) - 1, out_file);

    int64_t hash_pos = ftello64(out_file);

    //fseeko64(out_file, 16, SEEK_CUR);
    int64_t zero = 0;
    fwrite(&zero, sizeof(int64_t), 1, out_file);
    fwrite(&zero, sizeof(int64_t), 1, out_file); // writing empty hash

    int files_count = arc->archive_files_count + arc->included_files->count;
    fwrite(&files_count, sizeof(int), 1, out_file);


    MD5Context archive_hash;
    md5Init(&archive_hash);

    md5Update(&archive_hash, (uint8_t*) &files_count, sizeof(int));

    uint8_t file_hash[16];

//    uint64_t _buffer64[BUFFER_LENGTH / 8] = {0};
//    uchar* buffer = (uchar*)_buffer64;

    //uchar buffers[2][BUFFER_LENGTH] = {0};

    uchar** buffers = calloc(3, sizeof(uchar*));
    for (int i = 0; i < 3; ++i) {
        buffers[i] = calloc(BUFFER_LENGTH, sizeof(uchar));
    }

    for (int i = 0; i < arc->archive_files_count; ++i) {
        ArchiveFile info = get_file_info(out_file, i);
        md5Update(&archive_hash, info.file_hash, 16);
        archive_file_free(&info);

        skip_file(out_file); // skip existing files
    }

    arc->last_safe_eof = ftello64(out_file);
    //printf("\n{%lld}\n", arc->last_safe_eof);

    for (int i = 0; i < arc->included_files->count; ++i) {
        Str file_to_compress = dl_str_get(arc->included_files, i);
        FILE *file = fopen64(file_to_compress.value, "rb");
        dl_str_append(arc->processed_files, file_to_compress);

        if(file == NULL){
            throw(EXCEPTION_UNABLE_TO_OPEN_FILE, file_to_compress.value);
        }

        arc->compressing_total = 0; // because handling progress output when == 0

        // HANDLING FILE
        huffman_clear(coder);
        huffman_handle_file(coder, file);
        huffman_build_codes(coder);

        Str in_archive_file_name = str(get_file_name_from_path(file_to_compress.value));


        // COMPRESSING & WRITING FILE
//        make_and_write_file(
//                arc,
//                coder,
//                file,
//                out_file,
//                in_archive_file_name,
//                buffer,
//                BUFFER_LENGTH,
//                file_hash
//                );

        make_and_write_file__multithread(
                arc,
                coder,
                file,
                out_file,
                in_archive_file_name,
                buffers,
                BUFFER_LENGTH,
                3,
                file_hash
                );

        fclose(file);

        if(any_exceptions()){
            break;
        }

        arc->last_safe_eof = ftello64(out_file);
        //printf("\n{%lld}\n", arc->last_safe_eof);

        arc->archive_files_count++;

        md5Update(&archive_hash, file_hash, 16);
    }

    if(!any_exceptions()){
        md5Finalize(&archive_hash);
        fseeko64(out_file, hash_pos, SEEK_SET);

        fwrite(archive_hash.digest, sizeof(uint8_t), 16, out_file);


        fclose(out_file);

        arc->writing_file_stream = NULL;

        arc->work_stage = WORK_NONE;
    }

    huffman_free(coder);
}

// extract files with given ids and names
// if some id and name point to same file extract will once
// will create out_path if not exists
void archive_extract(Archive* arc, Str out_path, DynListInt* files_ids){
    HuffmanCoder* coder = huffman_coder_create();
    FILE* compressed = arc->archive_stream;

    if(!is_directory_exists(out_path.value)){
        if(!create_directory(out_path.value)){
            throw(EXCEPTION_UNABLE_TO_CREATE_FOLDER, out_path.value);
        }
    }

    char file_sign[sizeof(ARCHIVE_FILE_BEGIN)] = {0};
    fread(file_sign, sizeof(char), sizeof(ARCHIVE_FILE_BEGIN) - 1, compressed);
    ua_assert(strcmp(file_sign, ARCHIVE_FILE_BEGIN) == 0);

    uint8_t saved_hash[16];
    fread(saved_hash, sizeof(uint8_t), 16, compressed);

    int files_count;
    fread(&files_count, sizeof(int), 1, compressed);

    uchar buffer[BUFFER_LENGTH] = {0};
    uchar out_buffer[BUFFER_LENGTH*8] = {0};

    arc->work_stage = WORK_DECOMPRESSING;

    for (int i = 0; i < files_count; ++i) {

        if(any_exceptions())
            break;

        int64_t length;
        fread(&length, sizeof(int64_t), 1, compressed);

        if(dl_int_find(files_ids, i) != -1){
            fseeko64(compressed, -(int)(sizeof(int64_t)), SEEK_CUR); // move ptr because length did read
            ArchiveFile info = get_file_info(compressed, i);

            //printf("[BEGIN %lld]", ftello64(compressed));

            Str full_path = str_concat_path(out_path, info.file_name);

            FILE* decompressed = fopen64(full_path.value, "wb");

            if(decompressed == NULL){
                throw(EXCEPTION_UNABLE_TO_CREATE_FILE, full_path.value);
            }

            arc->writing_file_stream = decompressed;
            arc->writing_file_name = full_path;
            //printf("%s\n", full_path.value);

            dl_str_append(arc->processed_files, full_path);

            if(any_exceptions())
                break;

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
            arc->writing_file_stream = NULL;
        }
        else{
            fseeko64(compressed, length - (int)sizeof(length), SEEK_CUR); // because length include length length :)
        }
    }

    arc->work_stage = WORK_NONE;
}


// also update archive_hash field
// rewind FILE on end
// max_files == -1 for
DynListArchiveFile* archive_get_files(Archive* arc, int max_files){
    FILE* compressed = arc->archive_stream;

    char file_sign[sizeof(ARCHIVE_FILE_BEGIN)] = {0};
    ua_assert(
            sizeof(ARCHIVE_FILE_BEGIN) - 1 ==
            fread(file_sign, sizeof(char), sizeof(ARCHIVE_FILE_BEGIN) - 1, compressed)
            );

    ua_assert(strcmp(file_sign, ARCHIVE_FILE_BEGIN) == 0);

    uint8_t saved_hash[16];
    fread(saved_hash, sizeof(uint8_t), 16, compressed);

    for (int i = 0; i < 16; ++i) {
        arc->archive_hash[i] = saved_hash[i];
    }

    int files_count;
    fread(&files_count, sizeof(int), 1, compressed);

    DynListArchiveFile* infos = dl_arc_file_create(files_count);

    if(max_files == -1){
        max_files = files_count;
    }

    for (int i = 0; i < int_min(files_count, max_files); ++i) {
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

void archive_remove_files(Archive* arc, DynListInt* files_ids){

    if(files_ids->count == 0){
        return;
    }

    read_archive_header(arc);

    int64_t writing_ptr = ftello64(arc->archive_stream);
    int64_t reading_ptr = writing_ptr;
    int64_t end_of_reading = writing_ptr;


    uchar buffer[BUFFER_LENGTH] = {0};

    int original_files_count = arc->archive_files_count;

    for (int i = 0; i < original_files_count; ++i) {
        fseeko64(arc->archive_stream, reading_ptr, SEEK_SET);
        ArchiveFile info = get_file_info(arc->archive_stream, i);


        if(dl_int_find(files_ids, i) != -1) {
            reading_ptr += info.compressed_file_size;
            end_of_reading = reading_ptr;
            arc->archive_files_count--;

            continue;
        }
        else{
            end_of_reading = reading_ptr + info.compressed_file_size;
        }

        if(reading_ptr == writing_ptr){
            // don't need to copy
            writing_ptr += info.compressed_file_size;
            reading_ptr = end_of_reading;
        }

        archive_file_free(&info);



        while (reading_ptr != end_of_reading){
            int64_t block_length = int64_min(BUFFER_LENGTH, end_of_reading - reading_ptr);
            fseeko64(arc->archive_stream, reading_ptr, SEEK_SET);
            ua_assert(
                    block_length ==
                    fread(buffer, sizeof(uchar), block_length, arc->archive_stream)
                    );
            reading_ptr += block_length;

            fseeko64(arc->archive_stream, writing_ptr, SEEK_SET);
            uf_assert(
                    block_length ==
                    fwrite(buffer, sizeof(uchar), block_length, arc->archive_stream)
                    );
            writing_ptr += block_length;

        }

    }

    if(file_length(arc->archive_stream) == writing_ptr){
        return;
    }

    trunc_file(arc->archive_stream, writing_ptr);
    fclose(arc->archive_stream);// not working without this
    arc->archive_stream = fopen64(arc->file_name.value, "rb+");
    update_archive_header(arc);
}

void archive_free(Archive* arc){


    if(arc->writing_file_stream == arc->archive_stream){
        uf_assert(trunc_file(arc->archive_stream, arc->last_safe_eof));
        fclose(arc->archive_stream);
        arc->archive_stream = fopen64(arc->file_name.value, "rb+");

        update_archive_header(arc);
    }
    else if(arc->writing_file_stream != NULL){
        // extracting file corrupted
        fclose(arc->writing_file_stream);
        uf_assert(remove(arc->writing_file_name.value) == 0);
    }
}

void archive_file_free(ArchiveFile* file){
    str_free(file->file_name);
}
