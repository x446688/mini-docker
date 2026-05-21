#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>

const char *log_level_strings[] = {
  "DEBUG",
  "INFO",
  "WARNING",
  "ERROR",
  "CRITICAL",
};

/*
  Output information into a logfile.
  level - one of LOG_LEVEL_DEBUG, LOG_LVL_INFO, LOG_LVL_WARNING, LOG_LVL_ERROR, LOG_LVL_CRITICAL
  file - name of the logfile
  text - text with formatting support
*/
int logdoc(int level, const char* file, const char* text, ...)
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
    if (time(&now) == (time_t)-1) {
        printf("Error: Failed to get current time: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }
    
    struct tm *local = localtime(&now);
    if (local == NULL) {
        printf("Error: Failed to convert time to local time: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }
    
    char timeStr[32];
    if (strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", local) == 0) {
        printf("Error: Failed to format time string\n");
        fclose(fp);
        return -1;
    }

    va_list args;
    va_start(args, text);

    if (fprintf(fp, "[%s] [%s] ", timeStr, log_level_strings[level]) < 0 ||
        vfprintf(fp, text, args) < 0 ||
        fprintf(fp, "\n") < 0) 
    {
        printf("Error: Failed to write log prefix: %s\n", strerror(errno));
        va_end(args);
        fclose(fp);
        return -1;
    }
    
    va_end(args);
    
    if (fclose(fp) == EOF) {
        printf("Error: Failed to close log file: %s\n", strerror(errno));
        return -1;
    }

    return 1;
}
