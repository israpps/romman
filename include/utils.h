#ifndef INCLUDE_UTILS
#define INCLUDE_UTILS
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string>

#define RET_OK 0

#define SANEPATHSEP '/'
#define SANEPATHSEPS "/"
#if defined(_WIN32) || defined(WIN32)
#include <io.h>
#include <windows.h>
#define MKDIR(dir) mkdir(dir);
#define PATHSEP '\\'
#define PATHSEPS "\\"

// Enable ANSI escape codes in Windows (Windows 10 and above)
void enableANSIColors();

#else
#define MKDIR(dir) mkdir(dir, 0755);
#define PATHSEP '/'
#define PATHSEPS "/"
#endif
#define REDBOLD "\033[1;31m"
#define YELBOLD "\033[1;33m"
#define GRNBOLD "\033[1;32m"
#define RED "\033[0;31m"
#define DGREY "\033[0;90m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define WHITES "\033[1;97m"
#define DEFCOL "\033[0m"

struct memtrack_t {
    uint32_t fc = 0x0;  // free() count
    uint32_t mc = 0x0;  // malloc() count
    uint32_t rc = 0x0;  // realloc() count
};

extern memtrack_t MTR;

#define FREE(ptr)  \
    free(ptr);     \
    ptr = nullptr; \
    MTR.fc++;
#define MALLOC(size) \
    malloc(size);    \
    MTR.mc++;
#define REALLOC(ptr, want) \
    realloc(ptr, want);    \
    MTR.rc++;

namespace util {
uint32_t crc32(const void* data, size_t length);
uint16_t swapEndian16(uint16_t value);
uint32_t swapEndian32(uint32_t value);
void hexdump(const void* data, uint32_t size, bool hdr = false);
uint32_t GetSystemDate();
uint32_t GetTime();
int32_t GetLocalhostName(char* buffer, uint32_t BufferSize);
int getCWD(char* buffer, uint32_t BufferSize);
unsigned short int ConvertToBase16(unsigned short int value);
uint32_t GetFileModificationDate(const char* path);
bool SetFileModificationDate(const char* path, uint32_t date);
bool IsSonyRXModule(std::string path);
int GetSonyRXModInfo(std::string path, char* description, uint32_t MaxLength, uint16_t* version);
std::string Basename(std::string path);
std::string Dirname(std::string path);
void genericgaugepercent(int percent, std::string extra);
void genericgauge(float progress, std::string extra);
int dirExists(std::string path);
bool fileExists(std::string path);
};  // namespace util

#define DERROR(fmt, x...) fprintf(stderr, REDBOLD fmt DEFCOL, ##x);
#define DWARN(fmt, x...) fprintf(stderr, YELBOLD fmt DEFCOL, ##x);

#endif
