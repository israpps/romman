#include "debug.h"
#include "utils.h"
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#include <windef.h>
#endif

memtrack_t MTR; // rudimentary track down memory leaks?
void util::hexdump(const void* data, uint32_t size, bool hdr) {
    char ascii[17];
    uint32_t i, j;
    ascii[16] = '\0';
    if (hdr) {
        for (i = 0; i < 16; i++) {if (i==8) printf(" "); printf("%02X ", i);}
        printf("\n");
        for (i = 0; i < 23; i++) printf("---");
        printf("\n");
    }
    
    
    for (i = 0; i < size; ++i) {
        if (((unsigned char*)data)[i] == 0) printf(DGREY);
        printf("%02X ", ((unsigned char*)data)[i]);
        printf(DEFCOL);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i+1) % 8 == 0 || i+1 == size) {
            printf(" ");
            if ((i+1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i+1 == size) {
                ascii[(i+1) % 16] = '\0';
                if ((i+1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

uint32_t util::GetSystemDate() {
#if defined(_WIN32) || defined(WIN32)
    SYSTEMTIME SystemTime;
    GetSystemTime(&SystemTime);

    return (((unsigned int)ConvertToBase16(SystemTime.wYear)) << 16 | ConvertToBase16(SystemTime.wMonth) << 8 | ConvertToBase16(SystemTime.wDay));
#else
    time_t time_raw_format;
    struct tm *ptr_time;

    time(&time_raw_format);
    ptr_time = localtime(&time_raw_format);
    return (((unsigned int)ConvertToBase16(ptr_time->tm_year + 1900)) << 16 | ConvertToBase16(ptr_time->tm_mon + 1) << 8 | ConvertToBase16(ptr_time->tm_mday));
#endif
}

int32_t util::GetLocalhostName(char *buffer, uint32_t BufferSize) {
    int ret;
#if defined(_WIN32) || defined(WIN32)
    DWORD lpnSize;
    lpnSize = BufferSize;
    ret = (GetComputerNameA(buffer, &lpnSize) == 0 ? EIO : 0);
#else
    ret = gethostname(buffer, BufferSize);
#endif
    return ret;
}

int util::getCWD(char *buffer, uint32_t BufferSize) {
#if defined(_WIN32) || defined(WIN32)
   return (GetCurrentDirectoryA(BufferSize, buffer) == 0 ? EIO : 0);
#else
   if (getcwd(buffer, BufferSize) != NULL)
      return 0;
   else
      return EIO;
#endif
}


uint32_t util::GetFileCreationDate(const char *path)
{
#if defined(_WIN32) || defined(WIN32)
   HANDLE hFile;
   FILETIME CreationTime;
   SYSTEMTIME CreationSystemTime;

   if ((hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE) {
      GetFileTime(hFile, &CreationTime, NULL, NULL);
      CloseHandle(hFile);

      FileTimeToSystemTime(&CreationTime, &CreationSystemTime);
   } else
      GetSystemTime(&CreationSystemTime);

   return (((unsigned int)util::ConvertToBase16(CreationSystemTime.wYear)) << 16 | util::ConvertToBase16(CreationSystemTime.wMonth) << 8 | util::ConvertToBase16(CreationSystemTime.wDay));
#else
   struct tm *clock;                    // create a time structure
   struct stat attrib;                  // create a file attribute structure
   stat(path, &attrib);                 // get the attributes of afile.txt
   clock = gmtime(&(attrib.st_mtime));  // Get the last modified time and put it into the time structure
   return (((unsigned int)util::ConvertToBase16(clock->tm_year + 1900)) << 16 | util::ConvertToBase16(clock->tm_mon) << 8 | util::ConvertToBase16(clock->tm_mday));
#endif
}

bool util::IsSonyRXModule(std::string path) {
    return 0;
}


int util::GetSonyRXModInfo(std::string path, char *description, uint32_t MaxLength, uint16_t *version) {
    return 0;
}

std::string util::Basename(std::string path) {
    size_t x = 0;
        if ((x = path.find_last_of("/"
#if defined(_WIN32) || defined(WIN32)
        "\\"//windows cmd supports both separators, linux doesnt. match those behaviors
#endif
        )) != std::string::npos) {
            return path.substr(x);
        } else {
            return path;
        }
}

int util::dirExists(std::string path)
{
    struct stat info;
    if(stat(path.c_str(), &info) != 0)
        return ENOENT;
    else if(info.st_mode & S_IFDIR)
        return 0;
    else
        return ENOTDIR;
}

void util::genericgauge(float progress, std::string extra)
{
    int barWidth = 70;

    std::cout << "[";
    int pos = barWidth * progress;
    for (int i = 0; i < barWidth; ++i)
	{
	  if (i < pos)
		std::cout << "=";
	  else if (i == pos)
		std::cout << ">";
	  else
		std::cout << " ";
	}
    std::cout << "] " << int (progress * 100.0) << "% [" << extra <<"]        \r";
    std::cout.flush ();
}

//percentage represented on signed integer. values from 0-100
void util::genericgaugepercent(int percent, std::string extra) {
    util::genericgauge(percent*0.01, extra);
}

unsigned short int util::ConvertToBase16(unsigned short int value) {
    unsigned short int result;

    result = value + value / 10 * 0x06;
    result += value / 100 * 0x60;
    return (result + value / 1000 * 0x600);
}
