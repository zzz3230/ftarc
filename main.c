#include <stdio.h>
#include <time.h>
#include <synchapi.h>
#include <locale.h>
#include "compression_algorithms/huffman.h"
#include "pthread.h"


#define COLORER_IMPLEMENTATION
#include "utilities/colorer.h"
#include "archiver.h"

void sleep_ms(int milliseconds)
{
#ifdef WIN32
    Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = (milliseconds % 1000) * 1000000;
        nanosleep(&ts, NULL);
    #else
        usleep(milliseconds * 1000);
#endif
}

void* some_work(void* data){
    FILE* file = fopen("C:\\Users\\zzz32\\Downloads\\[SubsPlease] Liar Liar - 01 (1080p) [6E2AE377].mkv", "rb");

    clock_t  begin = clock();
    huffman_handle_file((HuffmanCoder*)data, file);
    printf("\nEnd %lf\n", (double)(clock() - begin) / CLOCKS_PER_SEC);

    fclose(file);

    return NULL;
}


void progress_bar(double percent, int width){
    int reached = (int)((double)width * percent);
    printf("[");
    for (int i = 0; i < reached; ++i) {
        putchar(219);
    }
    for (int i = 0; i < width - reached; ++i) {
        putchar(177);
    }
    printf("]");
}

//void* arc_work(void* data){
//    HuffmanCoder* coder = huffman_coder_create();
//    Archive* arc = (Archive*)data;
//    arc->current_coder = coder;
//
//
//    //FILE* file = fopen64("test2.txt", "rb");
//    //FILE* file = fopen64("img.png", "rb");
//
//    FILE* config_file = fopen("test_config.txt", "r");
//
//
//    char file_name_buffer[64];
//    arc->work_stage = WORK_HANDLING;
//    while (fscanf(config_file, "%s", file_name_buffer) == 1) {
//        Str file_to_compress = str(file_name_buffer);
//        FILE *file = fopen64(file_to_compress.value, "rb");
//        huffman_handle_file(coder, file);
//    }
//    huffman_build_codes(coder);
//
//    rewind(config_file);
//
//    FILE* out_file = fopen64("compressed", "wb");
//    uchar buffer[4096] = {0};
//
//    arc->work_stage = WORK_COMPRESSING;
//    int files_count = 0;
//    while (fscanf(config_file, "%s", file_name_buffer) == 1){
//        Str file_to_compress = str(file_name_buffer);
//        FILE *file = fopen64(file_to_compress.value, "rb");
//        dl_str_append(arc->processed_files, str_copy(file_to_compress));
//        make_and_write_file(arc, coder, file, out_file, file_to_compress, buffer, 4096, NULL);
//        fclose(file);
//        files_count++;
//    }
//    arc->work_stage = WORK_NONE;
//
//    fclose(out_file);
////    fclose(file);
//
//    uchar out_buffer[4096*8] = {0};
//    FILE* comp_file = fopen64("compressed", "rb");
//
//    sleep_ms(500);
//
//    arc->work_stage = WORK_DECOMPRESSING;
//
//    for (int i = 0; i < files_count; ++i) {
//
//        Str file_name = get_file_name(comp_file);
//
//        FILE* decomp_file = fopen64(str_concat(str("DEC_"), file_name).value, "wb");
//
//        dl_str_append(arc->processed_files, str_copy(file_name));
//
//        read_and_dec_file(
//                arc,
//                coder,
//                comp_file,
//                decomp_file,
//                buffer,
//                4096,
//                out_buffer,
//                4096*8
//        );
//        fclose(decomp_file);
//
//    }
//    arc->work_stage = WORK_NONE;
//
//    arc->all_work_finished = 1;
//    fclose(comp_file);
//}


void* arc_work_2(void* data){
    HuffmanCoder* coder = huffman_coder_create();
    Archive* myarc = (Archive*)data;
    myarc->current_coder = coder;

    archive_compress(myarc);

    myarc->all_work_finished = 1;

    fclose(myarc->archive_stream);
}

int main() {
    //printf("Hello, World!\n");

    //setlocale(LC_ALL, "Rus");

//    HuffmanCoder* coder = huffman_coder_create();
//    FILE* file = fopen64("huffman_code", "wb");
//    FILE* to_code = fopen64("img.png", "rb");
//    huffman_handle_file(coder, to_code);
//    huffman_build_codes(coder);
//    huffman_save_codes(coder, file);
//
//    printf("  %ld\n", ftell(file));
//
//    fclose(file);
//    file = fopen64("huffman_code", "rb");
//    HuffmanCoder* coder2 = huffman_coder_create();
//    huffman_load_codes(coder2, file);
//
//    for (int i = 0; i < 256; ++i) {
//        printf("%s\n", coder->codes[i].code);
//        if(strcmp(coder->codes[i].code, coder2->codes[i].code) != 0){
//            printf("BAD\n");
//            break;
//        }
//    }

//    Archive* myarc = archive_new(str("_test_arc"));
//
//
//    sleep_ms(500);
//
    Archive* myarc2 = archive_open(str("__arc.f2arc")); // "_test_arc"
    DynListArchiveFile* files = archive_get_files(myarc2);

    for (int i = 0; i < files->count; ++i) {
        ArchiveFile info = dl_arc_file_get(files, i);

        char file_size_str[32];
        pretty_bytes(file_size_str, info.original_file_size);

        printf("[%d] %s \t %.2lf%% \t %s\n",
               info.file_id,
               file_size_str,
               (double)info.compressed_file_size / (double)info.original_file_size * 100,
               info.file_name.value
               );
    }

    return 0;
    Archive* arc = archive_new(str("__arc.f2arc"));

    //archive_add_file(arc, str("Map11.wad.client"));
    archive_add_file(arc, str("27-B.txt"));
    archive_add_file(arc, str("gen.py"));
    archive_add_file(arc, str("track.mp3"));
    archive_add_file(arc, str("raw_image.dng"));
    //archive_add_file(arc, str("C:\\Users\\zzz32\\Downloads\\[SubsPlease] Liar Liar - 01 (1080p) [6E2AE377].mkv"));
    //archive_add_file(arc, str());


    arc->all_work_finished = 0;

    clock_t time_begin = clock();

    pthread_t thread1;

    pthread_create(&thread1, NULL, arc_work_2, arc);

//    color_init();
//    color_fg(stdout, COLOR_GREEN);
    char last_file_index = -1;
    enum e_work_stage cur = WORK_NONE;
    while (1){

        if(arc->all_work_finished){
            if(arc->processed_files->count <= last_file_index + 1){
                break;
            }
        }

        if(arc->work_stage == WORK_NONE){
            cur = WORK_NONE;
        }

        if(arc->processed_files->count > last_file_index + 1){
            last_file_index++;
            if(cur != arc->work_stage){
                cur = arc->work_stage;
                if(cur == WORK_COMPRESSING){
                    printf("\nbegin compressing\n\n");
                }
                if(cur == WORK_DECOMPRESSING){
                    printf("\nbegin decompressing\n\n");
                }
            }
            
            if(1){
                if(1){
                    //printf("OK\n");
                    //printf("\rOK                                                                \n");
                }
                printf("\r                                                                   ");
                printf("\rFILE: %s\n", dl_str_last(arc->processed_files).value);

            }
        }

        if(cur == WORK_HANDLING){
            printf("\rhandling | %.2lf  ",
                   arc->current_coder->read_progress * 100
            );

            progress_bar(arc->current_coder->read_progress, 32);
        }

        if(cur == WORK_COMPRESSING){

            double per = (double)arc->compressing_current / (double)arc->compressing_total;
            printf("\r%lld / %lld | %.2lf%%  ",
                   arc->compressing_current, arc->compressing_total,
                    per * 100
                   );

            progress_bar(per, 32);
        }
        if(cur == WORK_DECOMPRESSING){

            double per = (double)arc->decompressing_current / (double)arc->decompressing_total;
            printf("\r%lld / %lld | %.2lf%%  ",
                   arc->decompressing_current, arc->decompressing_total,
                   per * 100
            );

            progress_bar(per, 32);

        }

        fflush(stdout);
        sleep_ms(100);
    }

    printf("\r                                                                    ");

    clock_t time_end = clock();
    double time_spent = (double)(time_end - time_begin) / CLOCKS_PER_SEC;

    printf("\rtime spent: %.3lf sec", time_spent);

    color_reset(stdout);

    pthread_join(thread1, NULL);


    // 1010
//    Archive* arc = archive_new();
//    archive_add_file(arc, str("C:\\Users\\zzz32\\Downloads\\[SubsPlease] Liar Liar - 01 (1080p) [6E2AE377].mkv"));
//    archive_compress(arc, str("comp.f2arc"));
//    archive_free(arc);



//    int blocks_count = 0;
//    int total_size = 0;
//    while (1){
//        int bits_encoded = 0;
//        uchar buffer[4096] = {0};
//
//        huffman_encode_symbols(coder, file, buffer, 4096, &bits_encoded);
//
//        printf("block %d\n", bits_encoded);
//        fflush(stdout);
//
//        total_size += bits_encoded;
//
//        blocks_count++;
//        if(bits_encoded == 0){
//            break;
//        }
//    }
//
//    printf("%d\n", blocks_count);
//    printf("%lf KB\n", (float)total_size / 8 / 1024);

//    uchar out_buffer[4096] = {0};
//    int out_len = 0;
//    huffman_decode_symbols(coder, buffer, bits_encoded, out_buffer, &out_len);
//
//
//    FILE* out_file = fopen("data2", "wb");
//    fwrite(out_buffer, sizeof(char), out_len, out_file);



    return 0;
}
