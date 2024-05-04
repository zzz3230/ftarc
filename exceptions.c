
#include <stdlib.h>
#include "exceptions.h"

void throw(enum e_exception_code code, char* message){
    fprintf(stderr, "error ");

    FILE* out = stderr;

    switch (code) {
        case EXCEPTION_CANT_OPEN_FILE:
            fprintf(out, "cant open file");
        default:
            fprintf(out, "unknown");
    }

    if(message != NULL){
        fprintf(out, " : %s", message);
    }

    exit(EXIT_FAILURE);
}