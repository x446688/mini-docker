#ifndef LOGGER_H
#define  LOGGER_H
int logdoc(char* level, char* file, char* text);
#endif
static const char* log_level_strings [] = {
	"NONE", // 0
	"CRIT", // 1
	"WARN", // 2
	"NOTI", // 3
	" LOG", // 4
	"DEBG" // 5
};
