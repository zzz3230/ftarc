#include <math.h>
#include "utils.h"

void notnull_free(void* ptr){
    if(ptr != NULL) free(ptr);
}

int int_min(int a, int b){
    return a > b ? b : a;
}
int64_t int64_min(int64_t a, int64_t b){
    return a > b ? b : a;
}

int64_t file_length(FILE* file){
    int64_t cur = ftell(file);
    fseeko64(file, 0L, SEEK_END);
    int64_t file_size = ftell(file);
    fseeko64(file, cur, SEEK_SET);
    return file_size;
}

void print_hash(uint8_t *p){
    for(unsigned int i = 0; i < 16; ++i){
        printf("%02x", p[i]);
    }
    printf("\n");
}

void pretty_bytes(char* buf, int64_t bytes) {
    const char* suffixes[7];
    suffixes[0] = "B";
    suffixes[1] = "KB";
    suffixes[2] = "MB";
    suffixes[3] = "GB";
    suffixes[4] = "TB";
    suffixes[5] = "PB";
    suffixes[6] = "EB";
    int64_t s = 0; // which suffix to use
    double count = (double)bytes;
    while (count >= 1024 && s < 7)
    {
        s++;
        count /= 1024;
    }
    if (count - floor(count) == 0.0)
        sprintf(buf, "%d %s", (int)count, suffixes[s]);
    else
        sprintf(buf, "%.1f %s", count, suffixes[s]);
}

char* get_file_name_from_path(char* path)
{
    if( path == NULL )
        return NULL;

    char * pFileName = path;
    for(char * pCur = path; *pCur != '\0'; pCur++)
    {
        if( *pCur == '/' || *pCur == '\\' )
            pFileName = pCur+1;
    }

    return pFileName;
}