
#include <assert.h>

#include <errno.h>
#include <error.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>
#include <dirent.h>

#define ESPURIOUS 120
#define ECMDLINE 1

#define EXPECTED_LINE_LENGTH 1024
#define EXPECTED_HEADER_SIZE 4096
#define EXPECTED_HEADER_LINES 100

#define MAX_IDENTIFIER 63

// Verbosity levels
#define VERROR
#define VINTERACTIVE 1
#define VWARN 2
#define VINFO 3

enum operation {
    NOP = 0,
    LIST = 10,
    CAT = 20,
    STORE = 30
};

typedef struct {
    enum operation operation;
    bool show_usage;
    int verbose_level;
} arguments;

typedef struct {
    char *name;
    char *comment;
    int chunks;
    int64_t chunk_size;
    int64_t records;
    int64_t bytes;
} bucket_info;

typedef struct buckets_entry_struct {
    int pos;
    struct buckets_entry_struct *next;
    bucket_info info; 
} buckets_entry;

typedef struct {
    int count;
    buckets_entry *first;
} buckets_enumeration;

typedef struct string_list_struct {
    struct string_list_struct *next;
    char *string;
} string_list;
