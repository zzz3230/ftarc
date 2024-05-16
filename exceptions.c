
#include "exceptions.h"
#include "archiver.h"


enum e_exception_code current_exception = NOTHING;

Archive* loaded_archive = NULL;

void (*current_on_catch) (enum e_exception_code) = NULL;
enum e_exception_code current_catch_codes = NOTHING;
bool current_exit_anyway = false;

// only once at time
void try_begin(enum e_exception_code catch_codes, void (*on_catch) (enum e_exception_code)){
    current_catch_codes = catch_codes;
    current_on_catch = on_catch;
}

void set_loaded_archive(Archive* arc){
    loaded_archive = arc;
}

bool freeing = false;

void free_loaded_resources(){
    if(freeing){
        return; // prevent infinity recursion if ERROR in free funcs
    }
    freeing = true;

    if(loaded_archive){
        archive_free(loaded_archive);
    }
}

bool any_exceptions(){
    return current_exception != NOTHING;
}

void throw(enum e_exception_code code, char* message){

    current_exception = code;

    if(current_on_catch != NULL && current_catch_codes & code){
        current_on_catch(code);
        return;
    }

    FILE* out = stderr;

    color_fg(out, COLOR_BRED);

    fprintf(stderr, "\nERROR ");

    switch (code) {
        case EXCEPTION_UNABLE_TO_OPEN_FILE:
            fprintf(out, "unable to open file");
            break;
        case EXCEPTION_UNABLE_TO_CREATE_FILE:
            fprintf(out, "unable to create file");
            break;
        case EXCEPTION_FILE_ALREADY_EXISTS:
            fprintf(out, "file already exists");
            break;
        case EXCEPTION_NOT_AN_ARCHIVE:
            fprintf(out, "not an archive");
            break;
        case EXCEPTION_ARCHIVE_IS_CORRUPTED:
            fprintf(out, "archive is corrupted");
            break;
        case EXCEPTION_FATAL_ERROR:
            fprintf(out, "fatal error");
            break;
        case EXCEPTION_CANT_READ_FILE:
            fprintf(out, "cant read file");
            break;
        case EXCEPTION_STOP_SIGNAL:
            fprintf(out, "stopping");
            break;
        case EXCEPTION_INVALID_ARG:
            fprintf(out, "invalid arg");
            break;
        case EXCEPTION_INVALID_FILE_NAME:
            fprintf(out, "invalid file name");
            break;
        case EXCEPTION_CONFLICT_ARGS:
            fprintf(out, "conflict args");
            break;
        case EXCEPTION_NOT_FOUND_IN_ARCHIVE:
            fprintf(out, "file not found in archive");
            break;
        case EXCEPTION_UNABLE_TO_CREATE_FOLDER:
            fprintf(out, "unable to create folder");
            break;
        default:
            fprintf(out, "unknown");
            break;
    }

    if(message != NULL){
        fprintf(out, " : %s\n\n", message);
    }
    else{
        fprintf(out, "\n\n");
    }

    color_reset(out);

    free_loaded_resources();

    exit(EXIT_FAILURE);
}