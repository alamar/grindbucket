
#include "gbx.h"

string_list *string_list_append(string_list *list, char *string) {
    string_list *entry = malloc(sizeof(string_list));
    entry->next = NULL;
    entry->string = string;
    if (list != NULL) {
        assert(list->next == NULL);
        list->next = entry;
    }
    return entry;
}

string_list *string_list_consume(string_list *current) {
    assert(current);
    string_list *next = current->next;
    free(current->string);
    free(current);
    return next;
}
