
#include <stdlib.h>
#include "exceptions.h"

void throw(enum e_exception_code code, char* message){
    fprintf(stderr, "ERROR ");

    FILE* out = stderr;

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
        default:
            fprintf(out, "unknown");
            break;
    }

    if(message != NULL){
        fprintf(out, " : %s", message);
    }

    exit(EXIT_FAILURE);
}