#include <cstdio>

/* A dummy logfile module for snescom/nescom */

std::FILE *GetLogFile(const char*, const char*) { return NULL; }
