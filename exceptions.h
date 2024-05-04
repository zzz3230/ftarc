#ifndef FTARC_EXCEPTIONS_H_
#define FTARC_EXCEPTIONS_H_

#include <stdio.h>

enum e_exception_code {
    NOTHING,
    EXCEPTION_CANT_OPEN_FILE
};



void throw(enum e_exception_code code, char* message);

#endif //FTARC_EXCEPTIONS_H_
