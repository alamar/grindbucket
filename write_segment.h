


extern size_t write_segment_line(FILE *output, char *line);

extern size_t write_segment_header(FILE *output, char *name, char *value);

extern size_t write_segment_int_header(FILE *output, char *name, long long num_value);

extern size_t write_segment_padding(FILE *output, int length);

extern void write_segment_to_bucket(FILE *output, char *bucket, string_list *data, size_t flush_limit,
        int segment_ordinal, int segment_entries, segment_position spos, arguments *args);
