#ifndef PARGSER_H
#define PARGSER_H

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

int pargser(int argc, char *argv[], const char *format, ...);

#endif // PARGSER_H