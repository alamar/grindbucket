#include "gbx.h"

char *extract_identifier(char *source, char *kind, arguments *args) {
    char *identifier = malloc(MAX_IDENTIFIER + 1);
    int length = 0;
    bool started = false;
    bool ended = false;
    bool bad = false;
    int source_length = strlen(source);
    char c;
    for (int i = 0; i < source_length; i++) {
        c = source[i];
        if (c == ' ') {
            if (!started) {
                continue;
            } else {
                ended = true;
            }
        }
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') || c == '_' ||
            (!length && (
                (c >= '0' && c <= '9') ||
                // Somebody will surely like to have array-like or field-like identifiers
                c == '[' || c == ']' || c == '.')))
        {
            if (ended || length == MAX_IDENTIFIER) {
                bad = true;
                break;
            }
            started = true;
            identifier[length++] = c;
        } else {
            bad = true;
            break;
        }
    }
    if (bad || !started) {
        if (args->verbose_level >= VWARN) {
            identifier[length] = '\0';
            fprintf(stderr, "Warning: bad %s identifier %s%c\n", kind, identifier, (int) c);
        }
        free(identifier);
        return NULL;
    }
    identifier[length] = '\0';
    return identifier;
}
