#include "progress_display.h"
#include "utilities/colorer.h"

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
                color_fg(stdout, COLOR_BGREEN);
                if(cur == WORK_COMPRESSING){
                    printf("\n====COMPRESSING====\n\n");
                }
                if(cur == WORK_DECOMPRESSING){
                    printf("\n====EXTRACTING====\n\n");
                }
                if(cur == WORK_VALIDATING){
                    printf("\n====VALIDATING====\n\n");
                }
                color_reset(stdout);
            }

            printf("\r                                                                      ");

            color_fg(stdout, COLOR_BWHITE);
            printf("\rFILE: %s\n", dl_str_get(arc->processed_files, last_file_index).value);
            color_reset(stdout);
        }

        if(cur == WORK_HANDLING){
        }

        if(cur == WORK_COMPRESSING){
            if(arc->compressing_total == 0){
                // handling
                if(arc->current_coder){
                    printf("\rhandling | %.2lf  ",
                           arc->current_coder->read_progress * 100
                    );

                    progress_bar(arc->current_coder->read_progress, 32);
                }
            }
            else{
                // compressing
                double per = (double)arc->compressing_current / (double)arc->compressing_total;
                printf("\r%lld / %lld | %.2lf%%  ",
                       arc->compressing_current, arc->compressing_total,
                       per * 100
                );

                progress_bar(per, 32);
            }

        }
        if(cur == WORK_DECOMPRESSING || cur == WORK_VALIDATING){

            double per = (double)arc->decompressing_current / (double)arc->decompressing_total;
            printf("\r%lld / %lld | %.2lf%%  ",
                   arc->decompressing_current, arc->decompressing_total,
                   per * 100
            );

            progress_bar(per, 32);

        }

        fflush(stdout);

        if(!arc->all_work_finished){
            sleep_ms(100);
        }
    }

    printf("\r                                                                       ");


    if(arc->validating_status != VALIDATING_NOTHING){
        if(arc->validating_status == VALIDATING_INTACT){
            color_fg(stdout, COLOR_BGREEN);
            printf("\nArchive \"%s\" is intact\n", arc->file_name.value);
            color_reset(stdout);
        }
        else{
            color_fg(stdout, COLOR_BRED);
            printf("\nArchive \"%s\" is corrupted\n", arc->file_name.value);
            color_reset(stdout);
        }
    }


    color_fg(stdout, COLOR_BYELLOW);
    printf("\rTime spent: %.3lf sec\n", arc->time_spent);
    color_reset(stdout);
}
