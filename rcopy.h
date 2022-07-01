#ifndef __RCOPY__
#define __RCOPY__

#include "window.h"

int checkArgs(int argc, char * argv[]);
FILE *fileSetup(int argc, char **argv, SlidingWindow *window);

#endif