#include "progress_display.h"

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

void display_progress(Archive* arc){
    clock_t time_begin = clock();

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

            printf("\r                                                                   ");
            printf("\rFILE: %s\n", dl_str_last(arc->processed_files).value);

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
}