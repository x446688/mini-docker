#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>

int logdoc(const char* level, const char* file, const char* text, ...)
{
    FILE *fp = fopen(file, "a");
    if (fp == NULL) {
        if (errno == ENOENT) {
            printf("Error: Bad path or symbolic link.\n");
        } else if (errno == EACCES) {
            printf("Error: Permission denied.\n");
        } else {
            printf("Error: %s\n", strerror(errno));
        }
        return -1;
    }

    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", local);

    va_list args;
    va_start(args, text);

    // Prefix
    fprintf(fp, "[%s] [%s] ", timeStr, level);
    // Variadic print
    vfprintf(fp, text, args);
    fprintf(fp, "\n");

    va_end(args);
    fclose(fp);

    return 1;
}
