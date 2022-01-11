#include "../include/pargser.h"

#include <stdio.h>

int find_argument_address(int argc, char *argv[], const char *arg, size_t size) {

    for (int i = 0; i < argc; i++) {
        if (!strncmp(argv[i], arg, size)) return i;
    }

    return -1;
}

int pargser(int argc, char *argv[], const char *format, ...) {

    va_list args;

    va_start(args, format);

    int percent_symbol = 0, argv_index = -1;
    const char *start = format;

    int *int_arg;
    char *char_arg;
    char **charp_arg;

    for (; *format != '\0'; format++) {
        if (*format != '%' && !percent_symbol) continue;

        if (!percent_symbol) {
            percent_symbol = 1;
            argv_index = find_argument_address(argc, argv, start, format - start);
            continue;
        }

        switch (*format) {
            case '?':
                int_arg = va_arg(args, int*);
                *int_arg = argv_index != -1;
                break;
            case 'd':
                int_arg = va_arg(args, int*);
                if (argv_index == -1) break;
                if (argv_index + 1 == argc) return -1;
                *int_arg = atoi(argv[argv_index + 1]);
                break;
            case 's':
                char_arg = va_arg(args, char*);
                if (argv_index == -1) break;
                if (argv_index + 1 == argc) return -1;
                strcpy(char_arg, argv[argv_index + 1]);
                break;
            case '*':
                charp_arg = va_arg(args, char**);
                if (argv_index == -1) break;
                if (argv_index + 1 == argc) return -1;
                *charp_arg = argv[argv_index + 1];
                break;
            default:
                return -1;
        }

        percent_symbol = 0;
        argv_index = -1;
        start = format + 1;
    }

    return 0;
}