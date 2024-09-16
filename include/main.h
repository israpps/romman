#ifndef MAIN_H
#define MAIN_H
#include <stdint.h>
enum flags {
    SILENT = (1 << 0),
    VERBOSE = (1 << 1),
};

extern uint32_t Gflags;

#endif
