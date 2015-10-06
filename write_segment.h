
#define WS_NOSEEK -1
#define WS_NOPAD -1

extern size_t write_segment_padding(FILE *output, int length);

extern size_t write_header_for_segment(FILE *output, off_t offset, size_t pad_to, segment_header header);

extern void write_segment_to_bucket(FILE *output, segment_header header, string_list *data,
        size_t flush_limit, segment_position spos, arguments *args);
