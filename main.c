#include <stdio.h>
#include <time.h>
#include <synchapi.h>
#include <locale.h>
#include <unistd.h>
#include "compression_algorithms/huffman.h"
#include "pthread.h"
#include "args_parser.h"

#define COLORER_IMPLEMENTATION
#include "utilities/colorer.h"
#include "archiver.h"
#include "progress_display.h"

typedef struct s_thread_args{
    StartupArgs args;
    Archive* arc;
} ThreadArgs;


void print_help(){

}

void print_list(Archive* arc){
    DynListArchiveFile* files = archive_get_files(arc);

    printf("NUM  SIZE\t DATE\t\t\t RATIO \t\t NAME\n");
    printf("---- ----\t ----\t\t\t ----- \t\t ----\n");

    for (int i = 0; i < files->count; ++i) {
        ArchiveFile info = dl_arc_file_get(files, i);

        char file_size_str[256];
        pretty_bytes(file_size_str, info.original_file_size);

        struct tm *date = localtime(&info.add_date);
        char date_str[20];
        strftime(date_str, sizeof(date_str), "%x %H:%M", date);

        printf("[%d] %s\t %s \t %.2lf%% \t %s\n",
               info.file_id,
               file_size_str,
               date_str,
               (double)info.compressed_file_size / (double)info.original_file_size * 100,
               info.file_name.value
        );
    }

    printf("\n");
}

void* archive_work(void* th_args){
    StartupArgs args = ((ThreadArgs*)th_args)->args;
    Archive* arc = ((ThreadArgs*)th_args)->arc;

    if(args.action & ARC_ACTION_APPEND){
        for (int i = 0; i < args.files->count; ++i) {
            archive_add_file(arc, dl_str_get(args.files, i));
        }

        archive_save(arc);
    }
    else if(args.action & ARC_ACTION_EXTRACT){
        DynListInt* final_ids = args.numbers;

        if(final_ids == NULL){
            final_ids = dl_int_create(8);
            DynListArchiveFile* arc_files = archive_get_files(arc);
            for (int i = 0; i < arc_files->count; ++i) {
                if(dl_str_contains(args.files, dl_arc_file_get(arc_files, i).file_name)){
                    dl_int_append(final_ids, i);
                }
            }
        }

        archive_extract(arc, args.extract_path, final_ids);
    }

    arc->all_work_finished = 1;
}

int main(int argc, char *argv[]) {

    StartupArgs args = parse_args(argc, argv);

    if(args.action & ARC_ACTION_HELP){
        print_help();
        return 0;
    }


    Archive* arc;

    // LINKING WITH FILE
    if(args.action & ARC_ACTION_EXTRACT || args.action & ARC_ACTION_LIST || args.all_files & ARC_ACTION_VALIDATE){
        arc = archive_open(args.archive_name, false);
    }
    if(args.action & ARC_ACTION_DELETE){
        arc = archive_open(args.archive_name, true);
    }
    if(args.action & ARC_ACTION_APPEND){
        if(is_file_exists(args.archive_name.value)){
            arc = archive_open(args.archive_name, true);
        }
        else{
            arc = archive_new(args.archive_name);
        }
    }

    if(args.action & ARC_ACTION_LIST){
        print_list(arc);
        archive_free(arc);
        return 0;
    }

    ThreadArgs th_args;
    th_args.args = args;
    th_args.arc = arc;

    // STARTING WORK
    pthread_t work_thread;
    int th_creating_code = pthread_create(&work_thread, NULL, archive_work, &th_args);

    if(th_creating_code != 0){
        printf("Unable to start another thread. Code: %d\n", th_creating_code);
        printf("Progress output is not available.\n");
        printf("Wait for the end of the work...");

        archive_work(&th_args);
    }

    if(th_creating_code == 0){
        display_progress(arc);

        pthread_join(work_thread, NULL);
    }

    archive_free(arc);
    return 0;
}
