#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include "logger.h"

int logdoc(char* level, char* file, char* text)
{
  FILE *fp = fopen(file, "a");
  if (fp == NULL) {
    if (errno == ENOENT) {
        printf("Error: Bad path or symbolic link.\n");
    } else if (errno == EACCES) {
        printf("Error: Permission denied.\n");
        return -1;
    } else {
        printf("Error: %s\n", strerror(errno));
        return -1;
    }
  }

  time_t now;
  time(&now);                      
  struct tm *local = localtime(&now); 
  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", local);

  fprintf(fp, "[%s] [%s] %s\n", timeStr, level, text);
  fclose(fp);

  return 1;
}
