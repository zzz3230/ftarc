#include <stdio.h>
#include <time.h>
#include <synchapi.h>
#include <locale.h>
#include <unistd.h>
#include "compression_algorithms/huffman.h"
#include "pthread.h"
#include "args_parser.h"
#include "archiver.h"
#include "progress_display.h"
#include <signal.h>

#define COLORER_IMPLEMENTATION
#include "utilities/colorer.h"


typedef struct s_thread_args{
    StartupArgs args;
    Archive* arc;
} ThreadArgs;


void int_handler(int sig) {
    // ctrl-c; stopping
    throw(EXCEPTION_STOP_SIGNAL, "ctrl-c hit");
}

void print_help(){
    highlight_print("# FTARC 1.0 #\n\n");
    highlight_print("$ ftarc -[axdltfnho] arc.f2a [extract path] file1 file2 ...\n");
    highlight_print("[-a] append file to archive ([-ao] override archive)\n");
    highlight_print("[-x] extract files by ids [-xn] or names [-xf]; just [-x] extract ALL files\n");
    highlight_print("[-d] delete files by ids [-dn] or names [-df]\n");
    highlight_print("[-l] list files in archive\n");
    highlight_print("[-t] validate archive\n");
    highlight_print("[-h] this message\n\n");
}


void print_list(Archive* arc){
    DynListArchiveFile* files = archive_get_files(arc, -1);

    printf("ID   SIZE\t DATE\t\t\t RATIO \t\t NAME\n");
    printf("---- ----\t ----\t\t\t ----- \t\t ----\n");

    for (int i = 0; i < files->count; ++i) {
        ArchiveFile info = dl_arc_file_get(files, i);

        char file_size_str[256];
        pretty_bytes(file_size_str, info.original_file_size);

        struct tm *date = localtime(&info.add_date);
        char date_str[20];
        strftime(date_str, sizeof(date_str), "%x %H:%M", date);

        printf("[%d]  %s\t %s \t %.2lf%% \t %s\n",
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

    clock_t time_begin = clock();

    if(args.action & ARC_ACTION_APPEND){
        for (int i = 0; i < args.files->count; ++i) {
            archive_add_file(arc, dl_str_get(args.files, i));
        }

        archive_save(arc);
    }

    else if(args.action & ARC_ACTION_EXTRACT){
        DynListInt* final_ids = args.numbers;

        if(final_ids == NULL && args.files != NULL){
            // files by names
            final_ids = get_files_ids_by_names(arc, args.files);
        }
        else{
            // all files
            final_ids = dl_int_create(arc->archive_files_count);
            for (int i = 0; i < arc->archive_files_count; ++i) {
                dl_int_append(final_ids, i);
            }
        }

        archive_extract(arc, args.extract_path, final_ids);
    }

    else if(args.action & ARC_ACTION_VALIDATE){
        if(archive_validate(arc)){
            arc->validating_status = VALIDATING_INTACT;
        }
        else{
            arc->validating_status = VALIDATING_CORRUPTED;
        }
    }

    else if(args.action & ARC_ACTION_DELETE){
        DynListInt* final_ids = args.numbers;

        if(final_ids == NULL && args.files != NULL){
            // files by names
            final_ids = get_files_ids_by_names(arc, args.files);
        }
        archive_remove_files(arc, final_ids);
    }

    clock_t time_end = clock();
    double time_spent = (double)(time_end - time_begin) / CLOCKS_PER_SEC;
    arc->time_spent = time_spent;

    arc->all_work_finished = 1;
}

int main(int argc, char *argv[]) {

    signal(SIGINT, int_handler);

    color_init();

    StartupArgs args = parse_args(argc, argv);

    if(args.action & ARC_ACTION_HELP){
        print_help();
        return 0;
    }


    Archive* arc;

    // LINKING WITH FILE
    if(args.action & ARC_ACTION_EXTRACT || args.action & ARC_ACTION_LIST || args.action & ARC_ACTION_VALIDATE){
        arc = archive_open(args.archive_name, false);
    }
    if(args.action & ARC_ACTION_DELETE){
        arc = archive_open(args.archive_name, true);
    }
    if(args.action & ARC_ACTION_APPEND){
        if((args.action & ARC_ACTION_OVERRIDE) == 0 && is_file_exists(args.archive_name.value)){
            arc = archive_open(args.archive_name, true);
        }
        else{
            arc = archive_new(args.archive_name, args.action & ARC_ACTION_OVERRIDE);
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
