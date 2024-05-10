#ifndef UTILS_H_
#define UTILS_H_


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

void notnull_free(void* ptr);
int int_min(int a, int b);
int64_t int64_min(int64_t a, int64_t b);

int64_t file_length(FILE* file);
void print_hash(uint8_t *p);
void pretty_bytes(char* buf, int64_t bytes);

typedef unsigned char uchar;

char* get_file_name_from_path(char* path);
int is_digits(const char* str);

int is_file_exists(char* path);
void sleep_ms(int milliseconds);
int is_directory_exists(const char *path);

#endif