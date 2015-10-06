
extern string_list *parse_fields(char *fields_description, char *separator);

extern bool bucket_line_is_blank(char *line);

extern bool bucket_line_is_header(char *line);

extern char *read_bucket_line(FILE *stream, size_t *line_length, arguments *args) ;

extern void parse_bucket_header(FILE *stream, off_t offset, segment_header *result, arguments *args);

extern void cleanup_segment_header(segment_header *header);
