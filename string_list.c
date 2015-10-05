
#include "gbx.h"

string_list *string_list_append(string_list *tail, char *string) {
    string_list *entry = malloc(sizeof(string_list));
    entry->next = NULL;
    entry->string = string;
    if (tail) {
        assert(tail->next == NULL);
        tail->next = entry;
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

void string_list_discard(string_list *head) {
    while (head) {
        head = string_list_consume(head);
    }
}
