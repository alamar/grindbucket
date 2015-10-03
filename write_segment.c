
#include "gbx.h"

void write_segment_line(FILE *output, char *line, size_t *just_written) {
     *just_written = fwrite(line, 1, strlen(line), output);
     *just_written += fwrite("\n", 1, 1, output);
}

void write_segment_header(FILE *output, char *name, char *value, size_t *just_written) {
    // XXX Add value validation
    char *line = NULL;
    if (asprintf(&line, "#%s: %s", name, value) <= 0) {
        error(ESPURIOUS, errno, "Problem formatting header");        
    }
    write_segment_line(output, line, just_written);
    free(line);    
}

void write_segment_int_header(FILE *output, char *name, long long num_value, size_t *just_written) {
    char value[100];
    snprintf(value, 100, "%lld", num_value);
    write_segment_header(output, name, value, just_written);
}

void write_segment_padding(FILE *output, int length, size_t *just_written) {
    *just_written = 0;
    if (length == 0) {
        return;
    } else if (length == 1) {
        write_segment_line(output, "", just_written);
        return;
    }
    do {
        size_t just_just_written;
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
        write_segment_line(output, line, &just_just_written);
        *just_written += just_just_written;
        length -= line_length;
    } while (length > 0);
    assert(length == 0);
}

void write_segment_to_bucket(FILE *output, char *bucket, string_list *data,
        int segment_ordinal, int segment_entries, segment_position spos)
{
    size_t written = 0;
    size_t just_written = 0;

    write_segment_line(output, "##", &just_written);
    written += just_written;    
    write_segment_header(output, "Name", bucket, &just_written);
    written += just_written;
    write_segment_int_header(output, spos == ONLY ? "Entries" : "Segment-Entries", segment_entries, &just_written);
    written += just_written;
    if (spos != ONLY) {
        write_segment_int_header(output, "Segment-Ordinal", segment_ordinal, &just_written);
        written += just_written;
    }
    if (spos == MIDDLE) {
        write_segment_int_header(output, "Segment-Length", DEFAULT_SEGMENT_SIZE, &just_written);
        written += just_written;
        assert(written < DEFAULT_HEADER_SIZE);
        write_segment_padding(output, DEFAULT_HEADER_SIZE - written, &just_written);
        written += just_written;
    }
    while ((data = string_list_consume(data))) {
        write_segment_line(output, data->string, &just_written);
        written += just_written;
    }
    if (spos == MIDDLE) {
        assert(written <= DEFAULT_SEGMENT_SIZE);
        write_segment_padding(output, DEFAULT_SEGMENT_SIZE - written, &just_written);
        written += just_written;
        assert(written == DEFAULT_SEGMENT_SIZE);
    }
}
