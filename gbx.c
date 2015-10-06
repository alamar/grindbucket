#include "gbx.h"

char *empty_string = "";

bool parse_arguments(arguments *args, int argc, char **argv) {
    bool seen_operation = false;
    bool seen_bucket = false;
    bool fail = false;

    args->operation = NOP;
    args->bucket = NULL;
    args->show_usage = false;
    args->verbose_level = VERROR;
    args->fields = NULL;

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
                } else if (arg[pos] == 'F') {
                    if (pos + 1 == len && (i + 1 < argc)) {
                        args->fields = parse_fields(argv[++i], ",");
                    } else if (pos + 1 < len) {
                        args->fields = parse_fields(arg + pos + 1, ",");
                        if (args->fields != NULL) {
                            break;
                        }
                    }
                    if (!args->fields) {
                        fprintf(stderr, "-F requires list of fields\n");
                        fail = true;
                    }
                } else {
                    fprintf(stderr, "Unknown option: -%c\n", (int) arg[pos]);
                    fail = true;
                }
            }
        } else {
            if (strcmp(arg, "list") == 0) {
                seen_operation = true;
                args->operation = LIST;
            } else if (strcmp(arg, "cat") == 0) {
                seen_operation = true;
                args->operation = CAT;
            } else if (strcmp(arg, "store") == 0) {
                seen_operation = true;
                args->operation = STORE;
            } else if (strcmp(arg, "append") == 0) {
                seen_operation = true;
                args->operation = APPEND;
            } else {
                if (seen_operation && (args->operation == CAT || args->operation == STORE || args->operation == APPEND)) {
                    char *bucket = extract_identifier(arg, ID_EOL, "bucket", VERROR);
                    if (bucket != NULL) {
                        seen_bucket = true;
                        args->bucket = bucket;
                    } else {
                        fprintf(stderr, "Bad bucket name: %s\n", arg);
                        fail = true;
                    }
                } else {
                    fprintf(stderr, "Unknown argument: %s\n", arg);
                    fail = true;
                }
            }
        }
    }
    if (!seen_operation) {
        fprintf(stderr, "No operation specified!\n");
        fail = true;
    } else if (!seen_bucket && (args->operation == CAT || args->operation == STORE)) {
        fprintf(stderr, "No bucket name specified!\n");
        fail = true;
    }
    return !fail;
}

void print_usage(FILE *stream) {
    fprintf(stream, "USAGE: gbx [-v] {list|cat bucket|store bucket}\n");
}

void ellipsis_terminate(char *tail) {
    if (tail[3] != '\0') {
        tail[0] = '.';
        tail[1] = '.';
        tail[2] = '.';
        tail[3] = '\0';
    }
}

void print_bucket_info(segment_header *header, char *filename) {
    char pr_name[24];
    if (header->name) {
        strncpy(pr_name, header->name, 24);
        ellipsis_terminate(pr_name + 20);
    } else {
        snprintf(pr_name, 24, "<%s>", filename);
    }
    char pr_created[11] = "?   ";
    if (header->created) {
        strncpy(pr_created, header->created, 10);
        pr_created[10] = '\0';
    }
    char pr_entries[30];
    if (header->entries == 0) {
        strcpy(pr_entries, "?   ");
    } else {
        snprintf(pr_entries, 30, "%12lld", (long long int) header->entries);
    }
    char pr_fields[41] = "";
    if (header->fields) {
        strncat(pr_fields, header->fields->string, 40);
        for (string_list *field = header->fields->next; field; field = field->next) {
            strncat(pr_fields, ",", 40);
            strncat(pr_fields, field->string, 40);
            if (pr_fields[39] != '\0') {
                ellipsis_terminate(pr_fields + 36);
                break;
            }
        }
    } else {
        strcpy(pr_fields, "   ?");
    }

    char pr_comment[40] = "";
    if (header->comment) {
        strncpy(pr_comment, header->comment, 40);
        ellipsis_terminate(pr_comment + 36);
    }
    printf("%-23s %10s %12s %-39s %-39s\n", pr_name, pr_created, pr_entries, pr_fields, pr_comment);
}

void enumerate_buckets(arguments *args) {
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
            segment_header header;
            if (len > 3 && strcmp(filename + (len - 3), ".bx") == 0) {
                if (!found && args->verbose_level >= VINTERACTIVE) {
                    printf(" Name\t\t\tCreated     Entries\tFields\t\t\t\t\tComment\n");
                }
                FILE *stream = fopen(filename, "rb");
                if (!stream) {
                    perror("Opening file for enumerating");
                    continue;
                }
                parse_bucket_header(stream, 0, &header, args);
                print_bucket_info(&header, filename);
                cleanup_segment_header(&header);
                found = true;
                if (fclose(stream) != 0) {
                    perror("Closing file after enumerating");
                }
            }
        }
    } while (entry);
    if (!found) {
        printf("No buckets in current directory.");
    }
}

void cat_bucket(char *bucket, FILE *output, arguments *args) {
    char *filename = malloc(strlen(bucket) + 4);
    sprintf(filename, "%s.bx", bucket);
    FILE *input = fopen(filename, "rb");
    if (!input) {
        error(ENOTFOUND, errno, "Opening bucket for reading");
    }
    char *line;
    size_t line_length;
    while ((line = read_bucket_line(input, &line_length, args))) {
        if (!bucket_line_is_blank(line) && !bucket_line_is_header(line)) {
            fprintf(output, line);
            fprintf(output, "\n");
        }
        free(line);
    }
    if (fclose(input) != 0) {
        perror("Problem closing bucket after reading");
    }
    free(filename);
}

void store_bucket(FILE *input, char *bucket, bool append, arguments *args) {
    string_list *cache_head = NULL;
    string_list *cache_tail = NULL;
    size_t flush_limit = DEFAULT_SEGMENT_SIZE - DEFAULT_HEADER_SIZE;
    char *filename = malloc(strlen(bucket) + 4);
    struct stat existing;
    sprintf(filename, "%s.bx", bucket);
    if (stat(filename, &existing) < 0) {
        if (errno == ENOENT) {
            if (append && args->verbose_level >= VINTERACTIVE) {
                fprintf(stderr, "Notice: Append will create bucket\n");
            }
        } else {
            perror("Checking whether bucket exists");
        }
    } else {
        if (existing.st_size > 0) {
            // XXX Something more harsh like failing?
            if (!append && args->verbose_level >= VINTERACTIVE) {
                fprintf(stderr, "Notice: Store will overwrite bucket\n");
            }
        } else if (append && args->verbose_level >= VWARN && S_ISREG(existing.st_mode)) {
            fprintf(stderr, "Notice: Appending to empty bucket\n");
        }
    }
    FILE *output = fopen(filename, append ? "ab" : "w+b");
    // XXX Check if it already exists!
    if (!output) {
        error(ESPURIOUS, errno, "Opening bucket for writing");
    }
    char *line;
    size_t line_length = 0;
    size_t accumulated_length = 0;
    int64_t total_entries = 0;
    if (append) {
        size_t position = ftell(output);
        size_t rest = DEFAULT_SEGMENT_SIZE - (position % DEFAULT_SEGMENT_SIZE);
        if (rest < DEFAULT_HEADER_SIZE * 2) {
            assert(rest == write_segment_padding(output, rest));
        } else if (rest < DEFAULT_SEGMENT_SIZE) {
            flush_limit = rest - DEFAULT_HEADER_SIZE;
        }
    }

    segment_header header;
    header.lines = NULL;

    header.name = bucket;
    header.comment = NULL;
    header.created = NULL;
    header.fields = args->fields;
    header.segments = 1;
    header.segment_entries = 0;
    header.segment_length = 0;
    header.segment_ordinal = 1;
    header.entries = 0;

    while ((line = read_bucket_line(input, &line_length, args))) {
        if (bucket_line_is_header(line)) {
            // XXX ignore them for now
            continue;
        }
        if (accumulated_length + line_length > flush_limit) {
            assert(cache_head != NULL);
            write_segment_to_bucket(output, header, cache_head,
                    flush_limit + DEFAULT_HEADER_SIZE, MIDDLE, args);
            header.segment_entries = 0;
            header.segment_ordinal++;
            accumulated_length = 0;
            cache_head = NULL;
            cache_tail = NULL;
            flush_limit = DEFAULT_SEGMENT_SIZE - DEFAULT_HEADER_SIZE;
        }
        accumulated_length += line_length;
        cache_tail = string_list_append(cache_tail, line);
        header.segment_entries++;
        total_entries++;
        if (!cache_head) {
            cache_head = cache_tail;
        }
    }
    if (cache_head) {
        write_segment_to_bucket(output, header, cache_head, /* Should not be used */ -1, LAST, args);
    }

    if (header.segment_ordinal > 1) {
        segment_header first_header;
        size_t header_size = parse_bucket_header(output, 0, &first_header, args);
        bool problem = false;
        if (header_size <= 0 || header_size % DEFAULT_HEADER_SIZE != 0) {
            if (args->verbose_level >= VINTERACTIVE) {
                fprintf(stderr, "Warning: non-standard header size %d\n", (int) header_size);
            }
            problem = true;
        }
        if (first_header.entries > total_entries) {
            if (args->verbose_level >= VINTERACTIVE) {
                fprintf(stderr, "Warning: header declares more entries than accounted for\n");
            }
            problem = true;
        } else {
            first_header.entries = total_entries;
        }
        if (first_header.segments > header.segment_ordinal) {
            if (args->verbose_level >= VINTERACTIVE) {
                fprintf(stderr, "Warning: header declares more segments than accounted for\n");
            }
            problem = true;
        } else {
            first_header.segments = header.segment_ordinal;
        }
        if (!problem) {
            write_header_for_segment(output, 0, header_size, first_header);
        }
        cleanup_segment_header(&first_header);
    }
    if (fclose(output) != 0) {
        error(ESPURIOUS, errno, "Problem closing bucket after writing");
    }
    free(filename);
}

int main(int argc, char **argv) {
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
        enumerate_buckets(args);
    } else if (args->operation == CAT) {
        cat_bucket(args->bucket, stdout, args);
    } else if (args->operation == STORE) {
        store_bucket(stdin, args->bucket, false, args);
    } else if (args->operation == APPEND) {
        store_bucket(stdin, args->bucket, true, args);
    }
    free(args);
}
