#include "debug.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#include <windef.h>

void enableANSIColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Unable to get handle to stdout." << std::endl;
        return;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        std::cerr << "Error: Unable to get console mode." << std::endl;
        return;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) {
        std::cerr << "Error: Unable to set console mode." << std::endl;
        return;
    }
}
#endif

// Precomputed CRC32 lookup table
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
};

// CRC32 function
uint32_t util::crc32(const void* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* byteData = (const uint8_t*) data;

    for (size_t i = 0; i < length; ++i) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ byteData[i]) & 0xFF];
    }

    return crc ^ 0xFFFFFFFF;
}

uint16_t util::swapEndian16(uint16_t value) {
    return (value >> 8) | (value << 8);
}

uint32_t util::swapEndian32(uint32_t value) {
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8) & 0x0000FF00) |
           ((value << 8) & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
}

memtrack_t MTR;  // rudimentary track down memory leaks?
void util::hexdump(const void* data, uint32_t size, bool hdr) {
    char ascii[17];
    uint32_t i, j;
    ascii[16] = '\0';
    if (hdr) {
        for (i = 0; i < 16; i++) {
            if (i == 8)
                printf(" ");
            printf("%02X ", i);
        }
        printf("\n");
        for (i = 0; i < 23; i++)
            printf("---");
        printf("\n");
    }

    for (i = 0; i < size; ++i) {
        if (((unsigned char*) data)[i] == 0)
            printf(DGREY);
        printf("%02X ", ((unsigned char*) data)[i]);
        printf(DEFCOL);
        if (((unsigned char*) data)[i] >= ' ' && ((unsigned char*) data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*) data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i + 1) % 8 == 0 || i + 1 == size) {
            printf(" ");
            if ((i + 1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i + 1 == size) {
                ascii[(i + 1) % 16] = '\0';
                if ((i + 1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i + 1) % 16; j < 16; ++j) {
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

    return (((unsigned int) ConvertToBase16(SystemTime.wYear)) << 16 | ConvertToBase16(SystemTime.wMonth) << 8 | ConvertToBase16(SystemTime.wDay));
#else
    time_t time_raw_format;
    struct tm* ptr_time;

    time(&time_raw_format);
    ptr_time = localtime(&time_raw_format);
    return (((unsigned int) ConvertToBase16(ptr_time->tm_year + 1900)) << 16 | ConvertToBase16(ptr_time->tm_mon + 1) << 8 | ConvertToBase16(ptr_time->tm_mday));
#endif
}

uint32_t util::GetTime() {
#if defined(_WIN32) || defined(WIN32)
    SYSTEMTIME SystemTime;
    GetSystemTime(&SystemTime);

    return (((unsigned int) ConvertToBase16(SystemTime.wHour)) << 16 | ConvertToBase16(SystemTime.wMinute) << 8 | ConvertToBase16(SystemTime.wSecond));
#else
    time_t time_raw_format;
    struct tm* ptr_time;

    time(&time_raw_format);
    ptr_time = localtime(&time_raw_format);
    return (((unsigned int) ConvertToBase16(ptr_time->tm_hour)) << 16 | ConvertToBase16(ptr_time->tm_min) << 8 | ConvertToBase16(ptr_time->tm_sec));
#endif
}

int32_t util::GetLocalhostName(char* buffer, uint32_t BufferSize) {
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

int util::getCWD(char* buffer, uint32_t BufferSize) {
#if defined(_WIN32) || defined(WIN32)
    return (GetCurrentDirectoryA(BufferSize, buffer) == 0 ? EIO : 0);
#else
    if (getcwd(buffer, BufferSize) != NULL)
        return 0;
    else
        return EIO;
#endif
}

uint32_t util::GetFileModificationDate(const char* path) {
#if defined(_WIN32) || defined(WIN32)
    HANDLE hFile;
    FILETIME ModificationTime;
    SYSTEMTIME ModificationSystemTime;

    if ((hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, NULL, NULL, &ModificationTime);
        CloseHandle(hFile);

        FileTimeToSystemTime(&ModificationTime, &ModificationSystemTime);
    } else
        GetSystemTime(&ModificationSystemTime);

    return (((unsigned int) util::ConvertToBase16(ModificationSystemTime.wYear)) << 16 | util::ConvertToBase16(ModificationSystemTime.wMonth) << 8 | util::ConvertToBase16(ModificationSystemTime.wDay));
#else
    struct tm* clock;                // create a time structure
    struct stat attrib;              // create a file attribute structure
    if (stat(path, &attrib) != 0) {  // get the attributes of the file
        return 0;                    // return 0 if stat fails
    }
    clock = localtime(&(attrib.st_mtime));  // Get the last modified time and put it into the time structure
    return (((unsigned int) util::ConvertToBase16(clock->tm_year + 1900)) << 16 | util::ConvertToBase16(clock->tm_mon + 1) << 8 | util::ConvertToBase16(clock->tm_mday));
#endif
}

bool util::SetFileModificationDate(const char* path, uint32_t date) {
    // Create a buffer to store the hex string (8 characters for 32-bit hex)
    char hex_str[9];
    snprintf(hex_str, sizeof(hex_str), "%08X", date);  // Format date into hex string (8 digits)

    // Parse date: YYYYMMDD
    char year_str[5], month_str[3], day_str[3];

    // Extract year (first 4 symbols after '0x')
    snprintf(year_str, sizeof(year_str), "%c%c%c%c", hex_str[0], hex_str[1], hex_str[2], hex_str[3]);
    unsigned long year = strtoul(year_str, NULL, 10);

    // Extract month (next 2 symbols)
    snprintf(month_str, sizeof(month_str), "%c%c", hex_str[4], hex_str[5]);
    unsigned long month = strtoul(month_str, NULL, 10);

    // Extract day (last 2 symbols)
    snprintf(day_str, sizeof(day_str), "%c%c", hex_str[6], hex_str[7]);
    unsigned long day = strtoul(day_str, NULL, 10);
#if defined(_WIN32) || defined(WIN32)
    HANDLE hFile;
    FILETIME ModificationTime;
    SYSTEMTIME ModificationSystemTime;

    ModificationSystemTime.wYear = year;
    ModificationSystemTime.wMonth = month;
    ModificationSystemTime.wDay = day;
    ModificationSystemTime.wHour = 0;
    ModificationSystemTime.wMinute = 0;
    ModificationSystemTime.wSecond = 0;
    ModificationSystemTime.wMilliseconds = 0;

    if (!SystemTimeToFileTime(&ModificationSystemTime, &ModificationTime)) {
        return false;
    }

    if ((hFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE) {
        if (!SetFileTime(hFile, NULL, NULL, &ModificationTime)) {
            CloseHandle(hFile);
            return false;
        }
        CloseHandle(hFile);
    } else {
        return false;
    }

    return true;
#else
    struct utimbuf new_times;
    struct tm clock = {0};

    clock.tm_year = year - 1900;
    clock.tm_mon = month - 1;  // Month is 0-11
    clock.tm_mday = day;

    clock.tm_hour = 0;  // Set time to midnight
    clock.tm_min = 0;
    clock.tm_sec = 0;

    // Convert to time_t
    time_t modTime = mktime(&clock);
    if (modTime == -1) {
        return false;
    }

    // Set both access and modification times
    new_times.actime = modTime;   // Access time
    new_times.modtime = modTime;  // Modification time

    // Update file modification time
    if (utime(path, &new_times) != 0) {
        return false;
    }

    return true;
#endif
}

#include "ELF.h"

bool util::IsSonyRXModule(std::string path) {
    FILE* file;
    elf_header_t header;
    elf_shdr_t SectionHeader;
    int result;

    result = false;
    if ((file = fopen(path.c_str(), "rb")) != NULL) {
        fread(&header, 1, sizeof(elf_header_t), file);
        if (*(uint32_t*) header.ident == ELF_MAGIC && (header.type == ELF_TYPE_ERX2 || header.type == ELF_TYPE_IRX)) {
            unsigned int i;
            for (i = 0; i < header.shnum; i++) {
                fseek(file, header.shoff + i * header.shentsize, SEEK_SET);
                fread(&SectionHeader, 1, sizeof(elf_shdr_t), file);

                if ((SectionHeader.type == (SHT_LOPROC | SHT_LOPROC_EEMOD_TAB)) || (SectionHeader.type == (SHT_LOPROC | SHT_LOPROC_IOPMOD_TAB))) {
                    result = true;
                    break;
                }
            }
        }
        fclose(file);
    }
    return result;
}

int util::GetSonyRXModInfo(std::string path, char* description, uint32_t MaxLength, uint16_t* version) {
    FILE* file;
    int result;
    elf_header_t header;
    elf_shdr_t SectionHeader;

    result = -ENOENT;
    if ((file = fopen(path.c_str(), "rb")) != NULL) {
        fread(&header, 1, sizeof(elf_header_t), file);
        if (*(uint32_t*) header.ident == ELF_MAGIC && (header.type == ELF_TYPE_ERX2 || header.type == ELF_TYPE_IRX)) {
            unsigned int i;
            for (i = 0; i < header.shnum; i++) {
                fseek(file, header.shoff + i * header.shentsize, SEEK_SET);
                fread(&SectionHeader, 1, sizeof(elf_shdr_t), file);

                if (SectionHeader.type == (SHT_LOPROC | SHT_LOPROC_EEMOD_TAB) || SectionHeader.type == (SHT_LOPROC | SHT_LOPROC_IOPMOD_TAB)) {
                    void* buffer;
                    buffer = MALLOC(SectionHeader.size);
                    if (buffer != NULL) {
                        fseek(file, SectionHeader.offset, SEEK_SET);
                        fread(buffer, 1, SectionHeader.size, file);

                        if (SectionHeader.type == (SHT_LOPROC | SHT_LOPROC_IOPMOD_TAB)) {
                            *version = ((iopmod_t*) buffer)->version;
                            strncpy(description, ((iopmod_t*) buffer)->modname, MaxLength - 1);
                            description[MaxLength - 1] = '\0';
                        } else if (SectionHeader.type == (SHT_LOPROC | SHT_LOPROC_EEMOD_TAB)) {
                            *version = ((eemod_t*) buffer)->version;
                            strncpy(description, ((eemod_t*) buffer)->modname, MaxLength - 1);
                            description[MaxLength - 1] = '\0';
                        }

                        result = RET_OK;

                        FREE(buffer);
                    } else
                        result = -ENOMEM;
                    break;
                }
            }
        } else
            result = -EINVAL;

        fclose(file);
    } else
        result = -ENOENT;
    return result;
}

std::string util::Basename(std::string path) {
    // Remove trailing slashes
    path.erase(path.find_last_not_of("/"
#if defined(_WIN32) || defined(WIN32)
                                     "\\"
#endif
                                     ) +
               1);

    if (path.empty())
        return ".";

    size_t x = path.find_last_of(
        "/"
#if defined(_WIN32) || defined(WIN32)
        "\\"
#endif
    );
    if (x != std::string::npos) {
        return path.substr(x + 1);
    } else {
        return path;
    }
}

std::string util::Dirname(std::string path) {
    size_t x = 0;
    if ((x = path.find_last_of("/"
#if defined(_WIN32) || defined(WIN32)
                               "\\"  // windows cmd supports both separators, linux doesnt. match those behaviors
#endif
                               )) != std::string::npos) {
        return path.substr(0, x);
    } else {
        return ".";
    }
}

int util::dirExists(std::string path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
        return ENOENT;
    else if (info.st_mode & S_IFDIR)
        return 0;
    else
        return ENOTDIR;
}

bool util::fileExists(std::string path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

void util::genericgauge(float progress, std::string extra) {
    int barWidth = 70;

    std::cout << "[";
    int pos = barWidth * progress;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos)
            std::cout << "=";
        else if (i == pos)
            std::cout << ">";
        else
            std::cout << " ";
    }
    std::cout << "] " << int(progress * 100.0) << "% [" << extra << "]        \r";
    std::cout.flush();
}

// percentage represented on signed integer. values from 0-100
void util::genericgaugepercent(int percent, std::string extra) {
    util::genericgauge(percent * 0.01, extra);
}

unsigned short int util::ConvertToBase16(unsigned short int value) {
    unsigned short int result;

    result = value + value / 10 * 0x06;
    result += value / 100 * 0x60;
    return (result + value / 1000 * 0x600);
}
