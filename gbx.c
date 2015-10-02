#include "gbx.h"

int parse_arguments(arguments *args, int argc, char **argv) {
    bool seen_operation = false;
    bool fail = false;
    if (argc < 2) {
        args->show_usage = true;
        return true;
    }
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        int len = strlen(arg);
        if (len > 1 && arg[0] == '-' && arg[1] != '-') {
            for (int pos = 1; pos < len; pos++) {
                if (arg[pos] == 'h') {
                    args->show_usage = 1;
                } else if (arg[pos] == 'v') {
                    args->verbose_level++;
                } else {
                    fprintf(stderr, "Unknown option: -%c\n", (int) arg[pos]);
                }
            }
        } else {
            if (seen_operation) {
                printf("Unknown argument: %s\n", arg);
                fail = true;
            }
            if (strcmp(arg, "list") == 0) {
                seen_operation = true;
                args->operation = LIST;
            } else if (strcmp(arg, "cat") == 0) {
                seen_operation = true;
                args->operation = CAT;
            } else if (strcmp(arg, "store") == 0) {
                seen_operation = true;
                args->operation = STORE;
            } else {
                fprintf(stderr, "Unknown argument: %s\n", arg);
                fail = true;
            }
        }
    }
    return !fail;
}

void print_usage(FILE *stream) {
    fprintf(stream, "USAGE: gbx [-v] {list|cat|store}\n");
}

void enumerate_buckets(buckets_enumeration *buckets) {
    
}

int main (int argc, char **argv) {
    arguments *args = (arguments *) malloc(sizeof(arguments));
    if (!parse_arguments(args, argc, argv)) {
        print_usage(stderr);
        return 1;
    }
    if (args->show_usage) {
        print_usage(stdout);
        return 0;
    }
    free(args);
}
