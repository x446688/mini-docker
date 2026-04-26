#ifndef LOGGER_H
#define  LOGGER_H
#endif

int logdoc(const char* level, char* file, char* text, ...);

const char *log_level_strings[] = {
  "   DEBUG",
  "    INFO",
  "    WARN",
  "   ERROR",
  "CRITICAL",
};
