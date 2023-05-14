#ifndef _FILELS_H_ // This was just _FILE_H_, but that interfered with Cygwin
#define _FILELS_H_
#define MAX_FILE_TYPE 255
typedef struct {
    int size;
    void *data;
} file_data;

extern file_data *file_load(char *filename);
extern void file_free(file_data *filedata);

#endif
