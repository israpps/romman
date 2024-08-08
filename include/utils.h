#ifndef INCLUDE_UTILS
#define INCLUDE_UTILS
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <io.h>

#if defined(_WIN32) || defined(WIN32)
#define MKDIR(dir) mkdir(dir);
#define PATHSEP '\\'
#define PATHSEPS "\\"
#else
#define MKDIR(dir) mkdir(dir, 0755);
#define PATHSEP '/'
#define PATHSEPS "/"
#endif
#define SANEPATHSEP '/'
#define SANEPATHSEPS "/"

struct memtrack_t {
    uint32_t fc = 0x0;//free() count
    uint32_t mc = 0x0;//malloc() count
    uint32_t rc = 0x0;//realloc() count
};

extern memtrack_t MTR;

#define FREE(ptr) free(ptr); ptr = nullptr; MTR.fc++;
#define MALLOC(size)  malloc(size);  MTR.mc++;
#define REALLOC(ptr, want) realloc(ptr, want); MTR.rc++;

namespace util {
    void hexdump(const void* data, uint32_t size, bool hdr = false);
    uint32_t GetSystemDate();
    int32_t GetLocalhostName(char *buffer, uint32_t BufferSize);
    int getCWD(char *buffer, uint32_t BufferSize);
    unsigned short int ConvertToBase16(unsigned short int value);
    uint32_t GetFileCreationDate(const char *path);
    bool IsSonyRXModule(std::string path);
    int GetSonyRXModInfo(std::string path, char *description, uint32_t MaxLength, uint16_t *version);
    std::string Basename(std::string path);
    void genericgaugepercent(int percent, std::string extra);
    void genericgauge(float progress, std::string extra);
    int dirExists(std::string path);
}

#define REDBOLD "\033[1;31m"
#define YELBOLD "\033[1;33m"
#define GRNBOLD "\033[1;32m"

#define RED "\033[0;31m"
#define DGREY "\033[0;90m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"

#define WHITES "\033[1;97m"

#define DEFCOL "\033[0m"

#define DERROR(fmt, x...) fprintf(stderr, REDBOLD fmt DEFCOL, ##x);
#define DWARN(fmt, x...)  fprintf(stderr, YELLOW fmt DEFCOL, ##x);

#endif