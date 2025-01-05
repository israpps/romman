#ifndef MAIN_H
#define MAIN_H
#include <stdint.h>
enum flags {
    SILENT = (1 << 0),
    VERBOSE = (1 << 1),
};

extern uint32_t Gflags;

struct ConfFileEntry {
    std::string name;
    uint32_t offset;
    bool isFixed;
    std::string date;
    std::string version;
    std::string comment;
    std::string symlink;
};

#endif
