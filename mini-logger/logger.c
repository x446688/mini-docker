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

    va_list args;
    va_start(args, text);

    time_t now;
    time(&now);
    char timeStr[32];
    struct tm *local = localtime(&now);
    if (local == NULL) {
        fprintf(stderr, "Error: localtime() failed: %s\n", strerror(errno));
        va_end(args);
        fclose(fp);
        return -1;
    }

  if (strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", local) == 0) {
      fprintf(stderr, "Error: strftime() buffer too small or formatting failed\n");
      va_end(args);
      fclose(fp);
      return -1;
  }

  if (fprintf(fp, "[%s] [%s] ", timeStr, log_level_strings[level]) < 0 || // Prefix
      vfprintf(fp, text, args) < 0 || // Variadic print
      fprintf(fp, "\n") < 0) {
      va_end(args);
      if (fclose(fp) == EOF) {
          fprintf(stderr, "Error: fclose() failed: %s\n", strerror(errno));
          return -1; 
      } 
      return -1;
  }

    return 1;
};
