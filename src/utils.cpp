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
    DPRINTF("File modification date: %d-%d-%d\n", clock->tm_year + 1900, clock->tm_mon + 1, clock->tm_mday);
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
	FILE *file;
	elf_header_t header;
	elf_shdr_t SectionHeader;
	int result;

	result = false;
	if ((file = fopen(path.c_str(), "rb")) != NULL) {
		fread(&header, 1, sizeof(elf_header_t), file);
		if (*(uint32_t *)header.ident == ELF_MAGIC && (header.type == ELF_TYPE_ERX2 || header.type == ELF_TYPE_IRX)) {
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


int util::GetSonyRXModInfo(std::string path, char *description, uint32_t MaxLength, uint16_t *version) {

	FILE *file;
	int result;
	elf_header_t header;
	elf_shdr_t SectionHeader;

	result = -ENOENT;
	if ((file = fopen(path.c_str(), "rb")) != NULL) {
		fread(&header, 1, sizeof(elf_header_t), file);
		if (*(uint32_t *)header.ident == ELF_MAGIC && (header.type == ELF_TYPE_ERX2 || header.type == ELF_TYPE_IRX)) {
			unsigned int i;
			for (i = 0; i < header.shnum; i++) {
				fseek(file, header.shoff + i * header.shentsize, SEEK_SET);
				fread(&SectionHeader, 1, sizeof(elf_shdr_t), file);

				if (SectionHeader.type == (SHT_LOPROC | SHT_LOPROC_EEMOD_TAB) || SectionHeader.type == (SHT_LOPROC | SHT_LOPROC_IOPMOD_TAB)) {
					void *buffer;
                    buffer = MALLOC(SectionHeader.size);
					if (buffer != NULL) {
						fseek(file, SectionHeader.offset, SEEK_SET);
						fread(buffer, 1, SectionHeader.size, file);

						if (SectionHeader.type == (SHT_LOPROC | SHT_LOPROC_IOPMOD_TAB)) {
							*version = ((iopmod_t *)buffer)->version;
							strncpy(description, ((iopmod_t *)buffer)->modname, MaxLength - 1);
							description[MaxLength - 1] = '\0';
						} else if (SectionHeader.type == (SHT_LOPROC | SHT_LOPROC_EEMOD_TAB)) {
							*version = ((eemod_t *)buffer)->version;
							strncpy(description, ((eemod_t *)buffer)->modname, MaxLength - 1);
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
    size_t x = 0;
        if ((x = path.find_last_of("/"
#if defined(_WIN32) || defined(WIN32)
        "\\"//windows cmd supports both separators, linux doesnt. match those behaviors
#endif
        )) != std::string::npos) {
            return path.substr(x+1);
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
