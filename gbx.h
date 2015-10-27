
#include <assert.h>

#include <errno.h>
#include <error.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>

#include <unistd.h>

#define DEFAULT_HEADER_SIZE 4096
#define DEFAULT_SEGMENT_SIZE (1024 * 1024)

#define ESPURIOUS 120
#define ECMDLINE 1
#define ENOTFOUND 2
#define ENAUGHTY 3

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
    LIST = 100,
    CAT = 200,
    STORE = 300,
    APPEND = 350,
    SORT = 400
};

typedef enum {
    MIDDLE = 1,
    LAST = 2
} segment_position;

typedef enum {
    ONE_LINE,
    MULTILINE,
    COMMENT,
    BLANK
} header_kind;

typedef struct {
    enum operation operation;
    char *bucket;
    bool show_usage;
    int verbose_level;
    string_list *fields;
} arguments;

typedef struct {
    header_kind kind;
    char *name;
    char *value;

    char *raw;
    char *raw_second_line;
} header_line;

typedef struct header_lines_list_struct {
    struct header_lines_list_struct *next;
    header_line *line;
} header_lines_list;

typedef struct {
    header_lines_list *lines;

    char *name;
    char *created;
    char *comment;
    string_list *fields;
    int64_t entries;
    int64_t segment_entries;
    int64_t segment_ordinal;
    int64_t segments;
    int64_t segment_length;
} segment_header;

extern char *empty_string;

#include "identifier.h"

#include "read_bucket.h"

#include "write_segment.h"
