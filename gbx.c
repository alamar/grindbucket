#include "gbx.h"

bool parse_arguments(arguments *args, int argc, char **argv) {
    bool seen_operation = false;
    bool fail = false;
    if (argc < 2) {
        args->show_usage = true;
        return true;
    }
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        int len = strlen(arg);
        if (len > 1 && arg[0] == '-' && arg[1] != '-') {
            for (int pos = 1; pos < len; pos++) {
                if (arg[pos] == 'h') {
                    args->show_usage = 1;
                } else if (arg[pos] == 'v') {
                    args->verbose_level++;
                } else {
                    fprintf(stderr, "Unknown option: -%c\n", (int) arg[pos]);
                }
            }
        } else {
            if (seen_operation) {
                fprintf(stderr, "Unknown argument: %s\n", arg);
                fail = true;
            }
            if (strcmp(arg, "list") == 0) {
                seen_operation = true;
                args->operation = LIST;
            } else if (strcmp(arg, "cat") == 0) {
                seen_operation = true;
                args->operation = CAT;
            } else if (strcmp(arg, "store") == 0) {
                seen_operation = true;
                args->operation = STORE;
            } else {
                fprintf(stderr, "Unknown argument: %s\n", arg);
                fail = true;
            }
        }
    }
    return !fail;
}

void print_usage(FILE *stream) {
    fprintf(stream, "USAGE: gbx [-v] {list|cat|store}\n");
}

string_list *string_list_append(string_list *list, char *string) {
    string_list *entry = malloc(sizeof(string_list));
    entry->next = NULL;
    entry->string = string;
    if (list != NULL) {
        assert(list->next == NULL);
        list->next = entry;
    }
    return entry;
}

string_list *string_list_consume(string_list *current) {
    assert(current);
    string_list *next = current->next;
    free(current->string);
    free(current);
    return next;
}

char *read_bucket_line(FILE *stream, size_t *line_length, arguments *args) {
    char *line = NULL;
    size_t n = 0;
    *line_length = getline(&line, &n, stream);
    if (*line_length == -1) {
        if (!feof(stream)) {
            perror("Reading line");
        }
        if (line) {
            free(line);
        }
        return NULL;
    }
    if (args->verbose_level >= VWARN && *line_length > EXPECTED_LINE_LENGTH) {
        fprintf(stderr, "Warning: line longer than expected: %d\n", (int) *line_length);
    }
    if (line[*line_length - 1] == '\n') {
        line[*line_length - 1] = '\0';
    }
    return line;
}

bool bucket_line_is_blank(char *line) {
    assert(line);
    while (*line != '\0') {
        if (*line != '\t' && *line != ' ') {
            return false;
        }
        line++;
    }
    return true;
}

bool bucket_line_is_header(char *line) {
    assert(line);
    // Check whether line is sane!?
    return *line == '#';
}

string_list *read_header(FILE *stream, off_t offset, arguments *args) {
    int header_lines_found = 0;
    off_t header_size = 0;
    string_list *header_head = NULL;
    string_list *header = NULL;
    bool found_data = false;

    errno = 0;
    fseek(stream, offset, SEEK_SET);
    if (errno != 0) {
        perror("Seeking to header");
        return false;
    }
    char *line = NULL;
    do {
        size_t header_line_length;
        line = read_bucket_line(stream, &header_line_length, args);
        if (line) {
            if (bucket_line_is_blank(line) || bucket_line_is_header(line)) {
                header = string_list_append(header, line);
                if (!header_head) {
                    header_head = header;
                }
                header_lines_found++;
            } else {
                found_data = true;
                free(line);
                break;
            }
            header_size += header_line_length;
        }
    } while (line != NULL);
    if (args->verbose_level >= VWARN && header_size > EXPECTED_HEADER_SIZE) {
        fprintf(stderr, "Warning: header size larger than expected: %lld\n", (long long int) header_size);
    }
    if (args->verbose_level >= VWARN && header_lines_found >= EXPECTED_HEADER_LINES) {
        fprintf(stderr, "Warning: header contains more lines than expected: %d\n", header_lines_found);
    }
    if (args->verbose_level >= VWARN && !found_data) {
        fprintf(stderr, "Warning: no data after header\n");
    }
    return header_head;
}

void probe_bucket(bucket_info *info, char *filename, arguments *args) {
    info->name = strndup(filename, strlen(filename) - 3);
    FILE *stream = fopen(filename, "rb");
    string_list *header = read_header(stream, 0, args);

    char *name = NULL;
    int64_t entries = 0;
    int64_t segments = 1;
    int64_t segment_length = 0;
    char *created = NULL;
    char *comment = NULL;
    
    while (header) {
        char *value = header->string + 1; /* Skip heading # */
        if (args->verbose_level >= VINFO) {
            printf("%s\n", value);
        }
        if (strncmp("Name: ", value, 6) == 0) {
            if (name != NULL) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: name specified more than once\n");
                }
            } else {
                name = extract_identifier(value + 6, "name", args);
            }
        }
        if (strncmp("Created: ", value, 9) == 0) {
            if (created != NULL) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: creation date-time specified more than once\n");
                }
            } else {
                // XXX Add validation!
                created = value + 9;
            }
        }
        if (strncmp("Segments: ", value, 10) == 0) {
            if (segments != 1) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: segments count specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(value + 10, "%lld", &segments);
            }
        }
        if (strncmp("Entries: ", value, 9) == 0) {
            if (entries != 0) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: entries count specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(value + 9, "%lld", &entries);
            }
        }
        if (strncmp("Segment-Length: ", value, 16) == 0) {
            if (segment_length != 0) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: segment length specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(value + 16, "%lld", &segment_length);
            }
        }
        header = string_list_consume(header);
    }
    if (name != NULL) {
        printf("%s %10lld %s %10lld %10lld\n", name, entries, created == NULL ? "?" : created, segments, segment_length);
    }
    if (args->verbose_level >= VINFO) {
        printf("====\n");
    }
}

void enumerate_buckets(buckets_enumeration *buckets, arguments *args) {
    struct dirent *entry;
    DIR *current = opendir(".");
    bool found = false;
    if (!current) {
        error(ESPURIOUS, errno, "Opening directory for listing");
    }
    do {
        errno = 0;
        if (!(entry = readdir(current))) {
            if (errno != 0) {
                error(ESPURIOUS, errno, "Listing directories");
            }
        } else {
            char *filename = entry->d_name;
            int len = strlen(filename);
            if (len > 3 && strcmp(filename + (len - 3), ".bx") == 0) {
                if (!found && args->verbose_level >= VINTERACTIVE) {
                    printf(" Name Entries Created Segments Segment size Comment\n");
                }
                bucket_info info;
                probe_bucket(&info, filename, args);
                found = true;
            }
        }
    } while (entry);
}

int main (int argc, char **argv) {
    arguments *args = malloc(sizeof(arguments));
    if (!parse_arguments(args, argc, argv)) {
        print_usage(stderr);
        return ECMDLINE;
    }
    if (args->show_usage) {
        print_usage(stdout);
        return 0;
    }
    if (args->operation == LIST) {
        buckets_enumeration *buckets = malloc(sizeof(buckets_enumeration));
        enumerate_buckets(buckets, args);
        free(buckets);
    }
    free(args);
}
