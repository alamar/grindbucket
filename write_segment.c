
#include "gbx.h"

size_t write_segment_line(FILE *output, char *line) {
     size_t just_written = fwrite(line, 1, strlen(line), output);
     return just_written + fwrite("\n", 1, 1, output);
}

size_t write_segment_header(FILE *output, char *name, char *value) {
    // XXX Add value validation
    char *line = NULL;
    if (asprintf(&line, "#%s: %s", name, value) <= 0) {
        error(ESPURIOUS, errno, "Problem formatting header");        
    }
    size_t just_written = write_segment_line(output, line);
    free(line);
    return just_written;
}

size_t write_segment_int_header(FILE *output, char *name, long long num_value) {
    char value[100];
    snprintf(value, 100, "%lld", num_value);
    return write_segment_header(output, name, value);
}

size_t write_segment_padding(FILE *output, int length) {
    if (length == 0) {
        return 0;
    } else if (length == 1) {
        return write_segment_line(output, "");
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
        just_written += write_segment_line(output, line);
        length -= line_length;
    } while (length > 0);
    assert(length == 0);
    return just_written;
}

char *format_fields_header(string_list *fields) {
    char *field = fields->string;
    int fields_length = 3 + strlen(field);
    char *line_of_fields = malloc(fields_length);
    sprintf(line_of_fields, "# %s", field);

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

void write_segment_to_bucket(FILE *output, char *bucket, string_list *data,
        int segment_ordinal, int segment_entries, segment_position spos, arguments *args)
{
    size_t written = 0;

    written += write_segment_line(output, "##");
    written += write_segment_header(output, "Name", bucket);
    written += write_segment_int_header(output, spos == ONLY ? "Entries" : "Segment-Entries", segment_entries);
    if (spos != ONLY) {
        written += write_segment_int_header(output, "Segment-Ordinal", segment_ordinal);
    }
    if (spos == MIDDLE) {
        written += write_segment_int_header(output, "Segment-Length", DEFAULT_SEGMENT_SIZE);
    }
    if (args->fields) {
        written += write_segment_line(output, "#Fields");
        char *line_of_fields = format_fields_header(args->fields);
        written += write_segment_line(output, line_of_fields);
        free(line_of_fields);
    }
    if (spos == MIDDLE) {
        assert(written < DEFAULT_HEADER_SIZE);
        written += write_segment_padding(output, DEFAULT_HEADER_SIZE - written);
    }
    while ((data = string_list_consume(data))) {
        written += write_segment_line(output, data->string);
    }
    if (spos == MIDDLE) {
        assert(written <= DEFAULT_SEGMENT_SIZE);
        written += write_segment_padding(output, DEFAULT_SEGMENT_SIZE - written);
        assert(written == DEFAULT_SEGMENT_SIZE);
    }
}
