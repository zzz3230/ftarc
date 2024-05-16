#ifndef FTARC_EXCEPTIONS_H_
#define FTARC_EXCEPTIONS_H_

#include <stdio.h>
#include "archive_structs.h"
#include "utilities/colorer.h"
#include <stdlib.h>

#ifdef DEBUG
    #define u_assert(_Expression)\
 ((!!(_Expression)) || \
  (_assert(#_Expression,__FILE__,__LINE__),0))
#define ua_assert(_Expression)\
 ((!!(_Expression)) || \
  (_assert(#_Expression,__FILE__,__LINE__),0))
#define uf_assert(_Expression)\
 ((!!(_Expression)) || \
  (_assert(#_Expression,__FILE__,__LINE__),0))
#else
    #define uf_assert(exp) if(!(exp)) throw(EXCEPTION_FATAL_ERROR, #exp);
    #define ua_assert(exp) if(!(exp)) throw(EXCEPTION_ARCHIVE_IS_CORRUPTED, #exp);
#endif

enum e_exception_code {
    NOTHING                             = 1 << 0,
    EXCEPTION_UNKNOWN                   = 1 << 1,
    EXCEPTION_UNABLE_TO_OPEN_FILE       = 1 << 2,
    EXCEPTION_UNABLE_TO_CREATE_FILE     = 1 << 3,
    EXCEPTION_FILE_ALREADY_EXISTS       = 1 << 4,
    EXCEPTION_NOT_AN_ARCHIVE            = 1 << 5,
    EXCEPTION_ARCHIVE_IS_CORRUPTED      = 1 << 6,
    EXCEPTION_FATAL_ERROR               = 1 << 7,
    EXCEPTION_CANT_READ_FILE            = 1 << 8,
    EXCEPTION_STOP_SIGNAL               = 1 << 9,
    EXCEPTION_INVALID_ARG               = 1 << 10,
    EXCEPTION_INVALID_FILE_NAME         = 1 << 11,
    EXCEPTION_CONFLICT_ARGS             = 1 << 12,
    EXCEPTION_NOT_FOUND_IN_ARCHIVE      = 1 << 13,
    EXCEPTION_UNABLE_TO_CREATE_FOLDER   = 1 << 14,
};

bool any_exceptions();
void try_begin(enum e_exception_code catch_codes, void (*on_catch) (enum e_exception_code));
void throw(enum e_exception_code code, char* message);
void set_loaded_archive(Archive* arc);

#endif //FTARC_EXCEPTIONS_H_
