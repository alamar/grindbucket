#include "gbx.h"

char *empty_string = "";

string_list *parse_argument_fields(char *fields_description, char *separator) {
    char *first_comma = fields_description;
    char *next_comma;
    string_list *head = NULL;
    string_list *acc = NULL;
    bool finish = false;
    do {
        next_comma = strstr(first_comma, separator);
        if (!next_comma) {
            next_comma = fields_description + strlen(fields_description);
            finish = true;
        }
        char *field = extract_identifier(first_comma, (next_comma - first_comma), "field", VWARN);
        if (!field) {
            string_list_discard(head);
            return NULL;
        } else {
            acc = string_list_append(acc, field);
            if (!head) {
                head = acc;
            }
        }
        first_comma = next_comma + 1;
    } while (!finish);
    return head;
}

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
                        args->fields = parse_argument_fields(argv[++i], ",");
                    } else if (pos + 1 < len) {
                        args->fields = parse_argument_fields(arg + pos + 1, ",");
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

string_list *read_header_lines(FILE *stream, off_t offset, arguments *args) {
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

// Will mangle *line and return its bits in header_line
void parse_header_line(char *line, header_line *output) {
    assert(line[0] == '#');
    if (output->kind == MULTILINE && output->name != NULL && output->value == NULL &&
            line[1] == ' ')
    {
        output->value = line + 2;
        return;
    } else {
        output->kind = COMMENT;
        output->name = empty_string;
        output->value = line + 1;
    }

    if (line[1] < 'A' || line[1] > 'Z' || line[2] == '\0') {
        return;
    }

    // Maybe someday use extract_identifier?
    for (char *pos = line + 2; pos[1] != '\0'; pos++) {
        if (*pos == ':' && pos[1] == ' ') {
            output->kind = ONE_LINE;
            output->name = line + 1;
            output->value = pos + 2;
            *pos = '\0';
            return;
        }
        if (!((*pos >= 'a' && *pos <= 'z') || (*pos >= 'A' && *pos <= 'Z') || *pos == '-')) {
            return;
        }
    }
    output->kind = MULTILINE;
    output->name = line + 1;
    output->value = NULL;
}

void parse_bucket_header(char *filename, segment_header *result, arguments *args) {
    FILE *stream = fopen(filename, "rb");
    string_list *headers = read_header_lines(stream, 0, args);
    header_line header;

    result->name = NULL;
    result->created = NULL;
    result->comment = NULL;
    result->fields = NULL;
    result->entries = 0;
    result->segments = 1;
    result->segment_length = 0;

    long long int tmp = 0;

    while (headers) {
        parse_header_line(headers->string, &header);
        if (header.kind == MULTILINE && headers->next) {
            parse_header_line(headers->next->string, &header);
        }
        if (strcmp("Name", header.name) == 0) {
            if (result->name != NULL) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: name specified more than once\n");
                }
            } else {
                result->name = extract_identifier(header.value, ID_EOL, "bucket", args->verbose_level);
            }
        }
        if (strcmp("Comment", header.name) == 0) {
            if (result->comment != NULL) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: comment specified more than once\n");
                }
            } else {
                result->comment = strdup(header.value);
            }
        }
        if (strcmp("Fields", header.name) == 0) {
            if (result->fields != NULL) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: field names specified more than once\n");
                }
            } else {
                result->fields = parse_argument_fields(header.value, "\t");
            }
        }
        if (strcmp("Created", header.name) == 0) {
            if (result->created != NULL) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: creation date-time specified more than once\n");
                }
            } else {
                // XXX Add validation!
                result->created = strdup(header.value);
            }
        }
        if (strcmp("Segments", header.name) == 0) {
            if (result->segments != 1) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: segments count specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(header.value, "%lld", &tmp);
                result->segments = tmp;
            }
        }
        if (strcmp("Entries", header.name) == 0) {
            if (result->entries != 0) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: entries count specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(header.value, "%lld", &tmp);
                result->entries = tmp;
            }
        }
        if (strcmp("Segment-Length", header.name) == 0) {
            if (result->segment_length != 0) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: segment length specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(header.value, "%lld", &tmp);
                result->segment_length = tmp;
            }
        }
        headers = string_list_consume(headers);
        if (headers && header.kind == MULTILINE) {
            // XXX Only support one line for multiline comments :)
            headers = string_list_consume(headers);
        }
    }
}

void ellipsis_terminate(char *tail) {
    if (tail[3] != '\0') {
        tail[0] = '.';
        tail[1] = '.';
        tail[2] = '.';
        tail[3] = '\0';
    }
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
                parse_bucket_header(filename, &header, args);
                char pr_name[24];
                if (header.name) {
                    strncpy(pr_name, header.name, 24);
                    ellipsis_terminate(pr_name + 20);
                } else {
                    snprintf(pr_name, 24, "<%s>", filename);
                }
                char pr_created[11] = "?   ";
                if (header.created) {
                    strncpy(pr_created, header.created, 10);
                    pr_created[10] = '\0';
                }
                char pr_entries[30];
                if (header.entries == 0) {
                    strcpy(pr_entries, "?   ");
                } else {
                    snprintf(pr_entries, 30, "%12lld", (long long int) header.entries);
                }
                char pr_fields[41] = "";
                if (header.fields) {
                    strncat(pr_fields, header.fields->string, 40);
                    for (string_list *field = header.fields->next; field; field = field->next) {
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
                if (header.comment) {
                    strncpy(pr_comment, header.comment, 40);
                    ellipsis_terminate(pr_comment + 36);
                }
                printf("%-23s %10s %12s %-39s %-39s\n", pr_name, pr_created, pr_entries, pr_fields, pr_comment);
                found = true;
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
    }
    if (existing.st_size > 0) {
        // XXX Something more harsh like failing?
        if (!append && args->verbose_level >= VINTERACTIVE) {
            fprintf(stderr, "Notice: Store will overwrite bucket\n");
        }
    } else if (append && args->verbose_level >= VWARN && S_ISREG(existing.st_mode)) {
        fprintf(stderr, "Notice: Appending to empty bucket\n");
    }
    FILE *output = fopen(filename, append ? "ab" : "w+b");
    // XXX Check if it already exists!
    if (!output) {
        error(ESPURIOUS, errno, "Opening bucket for writing");
    }
    char *line;
    size_t line_length = 0;
    size_t accumulated_length = 0;
    int segment_ordinal = 1;
    int segment_entries = 0;
    if (append) {
        size_t position = ftell(output);
        size_t rest = DEFAULT_SEGMENT_SIZE - (position % DEFAULT_SEGMENT_SIZE);
        if (rest < DEFAULT_HEADER_SIZE * 2) {
            assert(rest == write_segment_padding(output, rest));
        } else if (rest < DEFAULT_SEGMENT_SIZE) {
            flush_limit = rest - DEFAULT_HEADER_SIZE;
        }
    }
    while ((line = read_bucket_line(input, &line_length, args))) {
        if (bucket_line_is_header(line)) {
            // XXX ignore them for now
            continue;
        }
        if (accumulated_length + line_length > flush_limit) {
            assert(cache_head != NULL);
            write_segment_to_bucket(output, bucket, cache_head, flush_limit + DEFAULT_HEADER_SIZE,
                    segment_ordinal++, segment_entries, MIDDLE, args);
            segment_entries = 0;
            accumulated_length = 0;
            cache_head = NULL;
            cache_tail = NULL;
            flush_limit = DEFAULT_SEGMENT_SIZE - DEFAULT_HEADER_SIZE;
        }
        accumulated_length += line_length;
        cache_tail = string_list_append(cache_tail, line);
        segment_entries++;
        if (!cache_head) {
            cache_head = cache_tail;
        }
    }
    if (cache_head) {
        write_segment_to_bucket(output, bucket, cache_head, /* Should not be used */ -1,
                segment_ordinal, segment_entries, segment_ordinal == 1 ? ONLY : LAST, args);
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
