
#include "gbx.h"

size_t write_segment_line(FILE *output, char *content, char *more_content) {
    assert(content);
    size_t just_written = fwrite(content, 1, strlen(content), output);
    if (more_content) {
        just_written += fwrite(more_content, 1, strlen(more_content), output);
    }
    return just_written + fwrite("\n", 1, 1, output);
}

size_t write_segment_header_line(FILE *output, char *name, char *value) {
    // XXX Add value validation
    char *line = NULL;
    if (asprintf(&line, "#%s: ", name) <= 0) {
        error(ESPURIOUS, errno, "Problem formatting header");        
    }
    size_t just_written = write_segment_line(output, line, value);
    free(line);
    return just_written;
}

// XXX We also have format_blabla
size_t write_segment_int_header(FILE *output, char *name, long long num_value) {
    char value[100];
    snprintf(value, 100, "%lld", num_value);
    return write_segment_header_line(output, name, value);
}

size_t write_segment_padding(FILE *output, int length) {
    if (length == 0) {
        return 0;
    } else if (length == 1) {
        return write_segment_line(output, "", NULL);
    }
    size_t just_written = 0;
    do {
        int line_length = length;
        if (line_length == 1025) {
            line_length = 1023;
        } else if (line_length > 1024) {
            line_length = 1024;
        }
        char line[line_length];
        line[0] = '#';
        memset(line + 1, ' ', line_length - 2);
        line[line_length - 1] = '\0';
        just_written += write_segment_line(output, line, NULL);
        length -= line_length;
    } while (length > 0);
    assert(length == 0);
    return just_written;
}

char *format_fields_header(string_list *fields) {
    char *field = fields->string;
    int fields_length = 1 + strlen(field);
    char *line_of_fields = malloc(fields_length);
    strcpy(line_of_fields, field);

    while ((fields = fields->next) != NULL) {
        field = fields->string;
        fields_length += strlen(field) + 1;
        line_of_fields = realloc(line_of_fields, fields_length);
        // XXX O(N^2)
        strcat(line_of_fields, "\t");
        strcat(line_of_fields, field);
    }
    return line_of_fields;
}

char *format_int_header(long long num_value) {
    char *value = malloc(100);
    snprintf(value, 100, "%lld", num_value);
    return value;
}

size_t write_header_for_segment(FILE *output, off_t offset, size_t pad_to, segment_header header) {
    size_t written = 0;

    if (offset != WS_NOSEEK) {
        fseek(output, offset, SEEK_SET);
    }
    written += write_segment_line(output, "##", NULL);
    header_lines_list *tail = header.lines;
    while (tail) {
        header_line *line = tail->line;
        if (line->kind == BLANK) {
            written += write_segment_line(output, line->raw, NULL);
        } else if (line->kind == COMMENT) {
            written += write_segment_line(output, "# ", (line->raw[1] == ' ') ? line->raw + 2 : line->raw + 1);
            if (line->raw_second_line) {
                written += write_segment_line(output, "# ", (line->raw_second_line[1] == ' ')
                        ? line->raw_second_line + 2 : line->raw_second_line + 1);
            }
        } else {
            char *name = line->name;
            char *value = line->value;
            bool need_free = false;
            if (header.name && strcmp(name, "Name") == 0) {
                value = header.name;
                header.name = NULL;
            } else if (header.created && strcmp(name, "Created")) {
                value = header.created;
                header.created = NULL;
            } else if (header.comment && strcmp(name, "Comment")) {
                value = header.comment;
                header.comment = NULL;
            } else if (header.fields && strcmp(name, "Fields")) {
                value = format_fields_header(header.fields);
                header.fields = NULL;
                need_free = true;
            } else if (header.entries > 0 && strcmp(name, "Entries")) {
                value = format_int_header(header.entries);
                header.entries = 0;
                need_free = true;
            } else if (header.segment_entries > 0 && strcmp(name, "Segment-Entries")) {
                value = format_int_header(header.segment_entries);
                header.segment_entries = 0;
                need_free = true;
            } else if (header.segment_ordinal > 0 && strcmp(name, "Segment-Ordinal")) {
                value = format_int_header(header.segment_ordinal);
                header.segment_ordinal = 0;
                need_free = true;
            } else if (header.segment_length > 0 && strcmp(name, "Segment-Length")) {
                value = format_int_header(header.segment_length);
                header.segment_length = 0;
                need_free = true;
            } else if (header.segments > 1 && strcmp(name, "Segments")) {
                value = format_int_header(header.segments);
                header.segment_length = 1;
                need_free = true;
            }
            if (line->kind == MULTILINE) {
                written += write_segment_header_line(output, name, value);
            } else {
                written += write_segment_line(output, "#", name);
                written += write_segment_line(output, "# ", value);
            }
            if (need_free) {
                free(value);
            }
        }
        tail = tail->next;
    }

    if (header.name) {
        written += write_segment_header_line(output, "Name", header.name);
    }
    if (header.created) {
        written += write_segment_header_line(output, "Created", header.name);
    }
    if (header.comment) {
        written += write_segment_line(output, "#Comment", NULL);
        written += write_segment_line(output, "# ", header.comment);
    }
    if (header.entries > 0) {
        written += write_segment_int_header(output, "Entries", header.entries);
    }
    if (header.segments > 1) {
        written += write_segment_int_header(output, "Segments", header.segments);
    }
    if (header.segment_entries > 0) {
        written += write_segment_int_header(output, "Segment-Entries", header.segment_entries);
    }
    if (header.segment_ordinal > 0) {
        written += write_segment_int_header(output, "Segment-Ordinal", header.segment_ordinal);
    }
    if (header.segment_length > 0) {
        written += write_segment_int_header(output, "Segment-Length", header.segment_length);
    }
    if (header.fields) {
        written += write_segment_line(output, "#Fields", NULL);
        char *line_of_fields = format_fields_header(header.fields);
        written += write_segment_line(output, "# ", line_of_fields);
        free(line_of_fields);
    }

    if (pad_to != WS_NOPAD) {
        assert(written <= pad_to);
        written += write_segment_padding(output, pad_to - written);
    }

    return written;
}

void write_segment_to_bucket(FILE *output, segment_header header, string_list *data,
        size_t flush_limit, segment_position spos, arguments *args)
{
    assert(data);
    header.segment_length = (spos == MIDDLE) ? flush_limit : 0;

    size_t written = 0;
    written += write_header_for_segment(output, WS_NOSEEK, (spos == MIDDLE)
            ? DEFAULT_HEADER_SIZE : WS_NOPAD, header);

    do {
        written += write_segment_line(output, data->string, NULL);
    } while ((data = string_list_consume(data)));

    if (spos == MIDDLE) {
        assert(written <= flush_limit);
        written += write_segment_padding(output, flush_limit - written);
        assert(written == flush_limit);
    }
}
