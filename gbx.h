
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

#define DEFAULT_HEADER_SIZE 4096
#define DEFAULT_SEGMENT_SIZE (1024 * 1024)

#define ESPURIOUS 120
#define ECMDLINE 1
#define ENOTFOUND 2

#define EXPECTED_LINE_LENGTH 1024
#define EXPECTED_HEADER_SIZE DEFAULT_HEADER_SIZE
#define EXPECTED_HEADER_LINES 100

#define MAX_IDENTIFIER 63

// Verbosity levels
#define VERROR 0
#define VINTERACTIVE 1
#define VWARN 2
#define VINFO 3

#include "string_list.h"

enum operation {
    NOP = 0,
    LIST = 10,
    CAT = 20,
    STORE = 30
};

typedef enum {
    ONLY = 0,
    MIDDLE = 1,
    LAST = 2
} segment_position;

typedef struct {
    enum operation operation;
    char *bucket;
    bool show_usage;
    int verbose_level;
    string_list *fields;
} arguments;

/*typedef struct {
    char *name;
    char *comment;
    int chunks;
    int64_t chunk_size;
    int64_t records;
    int64_t bytes;
} bucket_info;*/

#include "identifier.h"

#include "write_segment.h"
