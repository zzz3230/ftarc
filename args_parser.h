#ifndef FTARC_ARGS_PARSER_H_
#define FTARC_ARGS_PARSER_H_

#include "utilities/collections.h"

typedef enum e_archive_action{
    ARC_ACTION_UNKNOWN      = 1 << 1,
    ARC_ACTION_APPEND       = 1 << 2,
    ARC_ACTION_DELETE       = 1 << 3,
    ARC_ACTION_VALIDATE     = 1 << 4,
    ARC_ACTION_LIST         = 1 << 5,
    ARC_ACTION_EXTRACT      = 1 << 6,
    ARC_ACTION_USE_NUMBERS  = 1 << 7,
    ARC_ACTION_USE_FILES    = 1 << 8,
    ARC_ACTION_HELP         = 1 << 9
} ArchiveAction;

typedef struct s_StartupArgs{
    Str archive_name;
    Str extract_path;
    ArchiveAction action;
    DynListStr* files;
    DynListInt* numbers;
    bool all_files;
} StartupArgs;

StartupArgs parse_args(int argc, char *argv[]);

#endif //FTARC_ARGS_PARSER_H_
