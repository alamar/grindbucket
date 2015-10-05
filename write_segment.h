


extern void write_segment_line(FILE *output, char *line, size_t *just_written);

extern void write_segment_header(FILE *output, char *name, char *value, size_t *just_written);

extern void write_segment_int_header(FILE *output, char *name, long long num_value, size_t *just_written);

extern void write_segment_padding(FILE *output, int length, size_t *just_written);

extern void write_segment_to_bucket(FILE *output, char *bucket, string_list *data,
        int segment_ordinal, int segment_entries, segment_position spos, arguments *args);
