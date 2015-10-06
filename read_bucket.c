
#include "gbx.h"

string_list *parse_fields(char *fields_description, char *separator) {
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

string_list *read_header_lines(FILE *stream, off_t offset, size_t *header_size, arguments *args) {
    int header_lines_found = 0;
    *header_size = 0;
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
            if (line[0] == '#' && line[1] == '#' && line[2] == '\0') {
                // Technical "start of segment" mark
                free(line);
            } else if (bucket_line_is_blank(line) || bucket_line_is_header(line)) {
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
            *header_size += header_line_length;
        }
    } while (line != NULL);
    if (args->verbose_level >= VWARN && *header_size > EXPECTED_HEADER_SIZE) {
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
// If header is multiline, call twice on same header_line
void parse_header_line(char *line, header_line *output) {
    if (output->kind == MULTILINE && output->value == NULL) {
        assert(output->name != NULL);
        assert(output->raw != NULL);
        if (line && line[1] == ' ') {
            output->value = line + 2;
        } else {
            output->kind = COMMENT;
        }
        output->raw_second_line = line;
        return;
    }
    output->kind = COMMENT;
    output->name = empty_string;
    output->value = empty_string;
    output->raw = line;
    output->raw_second_line = NULL;
    if (line[0] != '#') {
        assert(bucket_line_is_blank(line));
        output->kind = BLANK;
        return;
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

// XXX This should be test covered HEAVILY
size_t parse_bucket_header(FILE *stream, off_t offset, segment_header *result, arguments *args) {
    size_t header_size;
    string_list *headers = read_header_lines(stream, offset, &header_size, args);
    header_line *header;
    header_lines_list *tail = NULL;

    result->lines = NULL;

    result->name = NULL;
    result->created = NULL;
    result->comment = NULL;
    result->fields = NULL;
    result->entries = 0;
    result->segments = 1;
    result->segment_length = 0;
    result->segment_entries = 0;
    result->segment_ordinal = 0;

    long long int tmp = 0;

    while (headers) {
        header = malloc(sizeof(header_line));
        parse_header_line(headers->string, header);
        string_list *next = headers->next;
        free(headers);
        headers = next;
        if (header->kind == MULTILINE) {
            parse_header_line(headers ? headers->string : NULL, header);
            if (headers) {
                next = headers->next;
                free(headers);
                headers = next;
            }
        }

        header_lines_list *new_tail = malloc(sizeof(header_lines_list));
        if (!tail) {
            result->lines = tail;
        } else {
            tail->next = new_tail;
        }
        tail = new_tail;
        tail->line = header;

        if (header->kind == COMMENT || header->kind == BLANK) {
            continue;
        }
        if (strcmp("Name", header->name) == 0) {
            if (result->name != NULL) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: name specified more than once\n");
                }
            } else {
                result->name = extract_identifier(header->value, ID_EOL, "bucket", args->verbose_level);
            }
        }
        if (strcmp("Comment", header->name) == 0) {
            if (result->comment != NULL) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: comment specified more than once\n");
                }
            } else {
                result->comment = strdup(header->value);
            }
        }
        if (strcmp("Fields", header->name) == 0) {
            if (result->fields != NULL) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: field names specified more than once\n");
                }
            } else {
                result->fields = parse_fields(header->value, "\t");
            }
        }
        if (strcmp("Created", header->name) == 0) {
            if (result->created != NULL) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: creation date-time specified more than once\n");
                }
            } else {
                // XXX Add validation!
                result->created = strdup(header->value);
            }
        }
        if (strcmp("Segments", header->name) == 0) {
            if (result->segments != 1) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: segments count specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(header->value, "%lld", &tmp);
                result->segments = tmp;
            }
        }
        if (strcmp("Entries", header->name) == 0) {
            if (result->entries != 0) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: entries count specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(header->value, "%lld", &tmp);
                result->entries = tmp;
            }
        }
        if (strcmp("Segment-Length", header->name) == 0) {
            if (result->segment_length != 0) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: segment length specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(header->value, "%lld", &tmp);
                result->segment_length = tmp;
            }
        }
        if (strcmp("Segment-Entries", header->name) == 0) {
            if (result->segment_entries != 0) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: segment entries count specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(header->value, "%lld", &tmp);
                result->segment_entries = tmp;
            }
        }
        if (strcmp("Segment-Ordinal", header->name) == 0) {
            if (result->segment_ordinal != 0) {
                if (args->verbose_level >= VWARN) {
                    fprintf(stderr, "Warning: segment ordinal specified more than once\n");
                }
            } else {
                // XXX Add validation!
                sscanf(header->value, "%lld", &tmp);
                result->segment_ordinal = tmp;
            }
        }
    }
    return header_size;
}

void cleanup_segment_header(segment_header *header) {
     free(header->name);
     free(header->created);
     free(header->comment);
     string_list_discard(header->fields);
     if (!header->lines) {
         return;
     }
     header_lines_list *tail = header->lines;
     do {
         header_line *line = tail->line;
         free(line->raw);
         if (line->raw_second_line) {
             free(line->raw_second_line);
         }
         header_lines_list *next = tail->next;
         free(tail);
         tail = next;
     } while (tail);
}
