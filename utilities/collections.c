//
// Created by zzz32 on 4/28/2024.
//

#include "collections.h"

int int_comparator(int a, int b){
    return a == b;
}

int str_comparator(Str a, Str b){
    assert(0 && "not implemented");
}

int arcfile_comparator(ArchiveFile a, ArchiveFile b){
    assert(0 && "not implemented");
}

DYN_LIST_IMPL_GENERATOR(Str, DynListStr, dl_str, str_comparator);
DYN_LIST_IMPL_GENERATOR(int, DynListInt, dl_int, int_comparator);
DYN_LIST_IMPL_GENERATOR(ArchiveFile, DynListArchiveFile, dl_arc_file, arcfile_comparator);
