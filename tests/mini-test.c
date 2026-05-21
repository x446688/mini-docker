#include <stdio.h>
#include <unistd.h>
#include "logger.h"
#define LOGFILE "/var/log/mini-docker.log"

int
main ()
{
  int counter = 0;
  while (1)
    {
      logdoc (LOG_LVL_INFO, LOGFILE, "Debug: %d", counter);
      counter++;
      sleep (2);
    }
}
