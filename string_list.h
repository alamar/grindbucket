
typedef struct string_list_struct {
    struct string_list_struct *next;
    char *string;
} string_list;

extern string_list *string_list_append(string_list *list, char *string);

extern string_list *string_list_consume(string_list *current);

extern void string_list_discard(string_list *list);
