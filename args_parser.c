#include "args_parser.h"

ArchiveAction char_to_action(char c){
    switch (c) {
        case 'a':
            return ARC_ACTION_APPEND;
        case 'd':
            return ARC_ACTION_DELETE;
        case 'x':
            return ARC_ACTION_EXTRACT;
        case 'l':
            return ARC_ACTION_LIST;
        case 't':
            return ARC_ACTION_VALIDATE;
        case 'f':
            return ARC_ACTION_USE_FILES;
        case 'n':
            return ARC_ACTION_USE_NUMBERS;
        case 'h':
            return ARC_ACTION_HELP;
        default:
            return ARC_ACTION_UNKNOWN;
    }
}


int is_require_uses(ArchiveAction action){
    return (action & ARC_ACTION_EXTRACT | action & ARC_ACTION_DELETE | action & ARC_ACTION_APPEND) != 0;
}
int is_conflict(ArchiveAction action){
    int operations_count =
            (action & ARC_ACTION_APPEND) != 0 +
            (action & ARC_ACTION_DELETE) != 0 +
            (action & ARC_ACTION_EXTRACT) != 0 +
            (action & ARC_ACTION_VALIDATE) != 0 +
            (action & ARC_ACTION_LIST) != 0;

    int uses =
            (action & ARC_ACTION_USE_FILES) != 0 +
            (action & ARC_ACTION_USE_NUMBERS) != 0;

    return operations_count != 1 && uses == 2;
}

StartupArgs parse_args(int argc, char *argv[]){

    StartupArgs args = {0};

    if(argc == 1){ // zero args == -h
        args.action = ARC_ACTION_HELP;
        return args;
    }

    for (int i = 1; i <= int_min(2, argc - 1); ++i) { // first two args
        if(argv[i][0] == '-'){
            for (int j = 1; argv[i][j]; ++j) {
                args.action |= char_to_action(argv[i][j]);
            }
        }
        else{
            args.archive_name = str(argv[i]);
        }
    }

    if(args.action == 0){
        u_assert(0);
    }

    if(args.action & ARC_ACTION_HELP){
        return args;
    }


    if(args.archive_name.length == 0){
        u_assert(0);
    }

    if(args.action & ARC_ACTION_UNKNOWN){
        u_assert(0);
    }

    if(is_conflict(args.action)){
        u_assert(0);
    }

    if(is_require_uses(args.action)){
        if(args.action & ARC_ACTION_USE_NUMBERS){
            args.numbers = dl_int_create(8);
        }
        if(args.action & ARC_ACTION_USE_FILES){
            args.files = dl_str_create(8);
        }

        if(
            (args.action & ARC_ACTION_EXTRACT) == 0 && // extract can be without others args
            args.files == NULL && args.numbers == NULL
        ){
            u_assert(0);
        }

        if(args.action & ARC_ACTION_APPEND && args.files == NULL){
            u_assert(0);
        }

        int continue_index = 3; // two args parsed

        if(args.action & ARC_ACTION_EXTRACT){
            if(argc < 4){
                u_assert(0);
            }

            args.extract_path = str(argv[3]);
            continue_index++;
        }

        if(args.files == NULL && args.numbers == NULL){
            args.all_files = true; // set field for export
        }

        for (int i = continue_index; i < argc; ++i) { // other args
            if(args.files != NULL){
                dl_str_append(args.files, str(argv[i]));
            }
            else if(args.numbers != NULL){

                if(!is_digits(argv[i])){
                    u_assert(0);
                }

                int value = strtol(argv[i], NULL, 10);
                dl_int_append(args.numbers, value);
            }
            else{
                u_assert(0);
            }
        }
    }

    return args;
}