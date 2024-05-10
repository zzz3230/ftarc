#ifndef FTARC_EXCEPTIONS_H_
#define FTARC_EXCEPTIONS_H_

#include <stdio.h>

enum e_exception_code {
    NOTHING,
    EXCEPTION_UNABLE_TO_OPEN_FILE,
    EXCEPTION_UNABLE_TO_CREATE_FILE,
    EXCEPTION_FILE_ALREADY_EXISTS,
    EXCEPTION_NOT_AN_ARCHIVE,
    EXCEPTION_ARCHIVE_IS_CORRUPTED
};



void throw(enum e_exception_code code, char* message);

#endif //FTARC_EXCEPTIONS_H_
