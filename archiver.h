#ifndef ARC_H_
#define ARC_H_

#include <stdint.h>
#include "utilities/utils.h"
#include "compression_algorithms/huffman.h"
#include "stdbool.h"
#include "utilities/md5.h"
#include "utilities/collections.h"
#include "utilities/timings.h"
#include "archiver_multithread.h"

#define ARCHIVE_FILE_EXTENSION ".f2a"
#define ARCHIVE_FILE_BEGIN "\002ftarc"
#define BUFFER_LENGTH 4096



Archive* archive_open(Str file_name, bool write_mode);
Archive* archive_new(Str file_name, bool override);

void archive_add_file(Archive* arc, Str file_name);
void archive_save(Archive* arc);
void archive_extract(Archive* arc, Str out_path, DynListInt* files_ids);
void archive_remove_files(Archive* arc, DynListInt* files_ids);
DynListArchiveFile* archive_get_files(Archive* arc, int max_files);

DynListInt* get_files_ids_by_names(Archive* arc, DynListStr* files);
void archive_free(Archive* arc);
ArchiveFile get_file_info(FILE* stream, int file_id);
bool archive_validate(Archive* arc);
void archive_file_free(ArchiveFile* file);
#endif