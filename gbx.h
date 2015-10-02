#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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
