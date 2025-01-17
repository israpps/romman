#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include "version.h"
#include "debug.h"
#include "rom.h"
#include "utils.h"
#include "main.h"

const std::string buildate = __DATE__ " " __TIME__;

uint32_t Gflags = 0x0;
#define VERB() !(Gflags & SILENT)
int submain(int argc, char** argv);
int help();

struct ConfFileEntry {
    std::string name;
    uint32_t offset;
    bool isFixed;
    std::string date;
    std::string version;
    std::string comment;
};

std::vector<ConfFileEntry> parseConfFile(const std::string& confFilePath) {
    std::vector<ConfFileEntry> entries;
    std::ifstream confFile(confFilePath);
    std::string line;

    while (std::getline(confFile, line)) {
        if (line.empty() || line[0] == '#')
            continue;  // Skip empty lines and comments

        std::istringstream ss(line);
        std::string name, offsetStr, date, version, comment;
        uint32_t offset = 0;
        bool isFixed = false;

        std::getline(ss, name, ',');
        std::getline(ss, offsetStr, ',');
        std::getline(ss, date, ',');
        std::getline(ss, version, ',');
        std::getline(ss, comment, ',');
        if (name == "ROMDIR")
            continue;

        if (!offsetStr.empty() && offsetStr != "-") {
            offset = std::stoul(offsetStr);
            // DPRINTF("offset: %u\n\n\n", offset);
            isFixed = true;
        }

        entries.push_back({name, offset, isFixed, date, version, comment});
        // DPRINTF("name: %s, offset: %u, isFixed: %d, date: %s, version: %s, comment: %s\n", name.c_str(), offset, isFixed, date.c_str(), version.c_str(), comment.c_str());
    }

    return entries;
}

int generateRomFromConf(const std::string& confFilePath, const std::string& romFilePath, const std::string& folderPath) {
    rom ROMIMG;
    // int ret = ROMIMG.CreateBlank(romFilePath, util::Basename(confFilePath), folderPath);
    int ret = ROMIMG.CreateBlank(romFilePath);
    if (ret != RET_OK)
        return ret;

    auto entries = parseConfFile(confFilePath);
    // DPRINTF("entries.size(): %lu\n", entries.size());
    unsigned int RomdirSize = (entries.size() + 2) * sizeof(rom::DirEntry);

    unsigned int TotalExtInfoSize = 0;
    // DPRINTF("rom comment: %s\n", ROMIMG.comment);
    unsigned int CommentLengthRounded = (strlen(ROMIMG.comment) + 1 + 3) & ~3;
    // DPRINTF("rom comment length rounded: %u\n", CommentLengthRounded);
    FILE* F;
    for (const auto& entry : entries) {
        std::string filePath = folderPath + "/" + entry.name;
        if ((F = fopen(filePath.c_str(), "rb")) != NULL) {
            fclose(F);
            if (util::IsSonyRXModule(filePath)) {
                char ModuleDescription[32] = {0};
                uint16_t FileVersion;
                if ((ret = util::GetSonyRXModInfo(filePath, ModuleDescription, sizeof(ModuleDescription), &FileVersion)) == RET_OK) {
                    if (strlen(ModuleDescription) > 0)
                        TotalExtInfoSize += sizeof(rom::ExtInfoFieldEntry) + (strlen(ModuleDescription) + 1 + 3) & ~3;
                    if (FileVersion > 0)
                        TotalExtInfoSize += sizeof(rom::ExtInfoFieldEntry);
                }
            } else if (!entry.version.empty())
                TotalExtInfoSize += sizeof(rom::ExtInfoFieldEntry);

            if (entry.isFixed) {
                TotalExtInfoSize += sizeof(rom::ExtInfoFieldEntry);
                RomdirSize += sizeof(rom::DirEntry);
            }
            if (entry.date != "-")
                TotalExtInfoSize += sizeof(rom::ExtInfoFieldEntry) + sizeof(uint32_t);
        } else {
            DERROR("Could not open file %s\n", filePath.c_str());
            return -ENOENT;
        }
    }
    TotalExtInfoSize += CommentLengthRounded + (2 * sizeof(rom::ExtInfoFieldEntry));
    // DPRINTF("RomdirSize: %u, TotalExtInfoSize: %u\n", RomdirSize, TotalExtInfoSize);
    TotalExtInfoSize = (TotalExtInfoSize + 0xF) & ~0xF;  // Align
    unsigned int currentOffset = 0;
    for (const auto& entry : entries) {
        std::string filePath = folderPath + "/" + entry.name;
        if ((F = fopen(filePath.c_str(), "rb")) == NULL) {
            DERROR("Could not open file %s\n", filePath.c_str());
            return -ENOENT;
        }
        fseek(F, 0, SEEK_END);
        uint32_t FileSize = ftell(F);
        fclose(F);

        bool isDate = entry.date != "-";
        uint16_t version = 0;
        if (!entry.version.empty())
            version = std::stoul(entry.version, nullptr, 16);

        if (strcmp(entry.name.c_str(), "RESET") == 0) {
            ret = ROMIMG.addFile(filePath, true, isDate, version);
            if (ret != RET_OK)
                break;
            currentOffset += (FileSize + 0xF) & ~0xF;
            currentOffset += RomdirSize + TotalExtInfoSize;
        } else if (entry.isFixed) {
            if (currentOffset <= entry.offset) {
                // DPRINTF("entry.name: %s, currentOffset: %u, entry.offset: %u\n", entry.name.c_str(), currentOffset, entry.offset);
                ret = ROMIMG.addDummy("-", entry.offset - currentOffset);
                if (ret != RET_OK)
                    break;
                currentOffset = entry.offset;
                ret = ROMIMG.addFile(filePath, true, isDate, version);
                if (ret != RET_OK)
                    break;
                currentOffset += (FileSize + 0xF) & ~0xF;
            } else {
                DERROR("Invalid offset for file %s\n", entry.name.c_str());
                DERROR("currentOffset: %u, entry.offset: %u\n", currentOffset, entry.offset);
                ret = -EINVAL;
                break;
            }
        } else {
            ret = ROMIMG.addFile(filePath, false, isDate, version);
            if (ret != RET_OK)
                break;
            currentOffset += (FileSize + 0xF) & ~0xF;
        }
    }

    if (ret == RET_OK)
        ret = ROMIMG.write(romFilePath);
    return ret;
}

int main(int argc, char** argv) {
    int ret = RET_OK;
    rom ROMIMG;
#if defined(_WIN32) || defined(WIN32)
    enableANSIColors();
#endif
    if (argc < 2) {
        return help();
    } else if (argc == 2) {
        if (!ROMIMG.open(argv[1]))
            return ROMIMG.displayContents();
        else
            DERROR("Could not find ROMFS filesystem on image\n");
    } else {
        int opbegin = -ENOENT;  // where the operation flag was found
        for (int c = 1; c < argc; c++) {
            // optional args
            if (!strcmp(argv[c], "--silent"))
                Gflags |= SILENT;
            if (!strcmp(argv[c], "-h")) {
                return help();
            } else if (!strcmp(argv[c], "--verbose") && VERB())
                Gflags |= VERBOSE;  // silent has priority
            else if (!strcmp(argv[c], "-g"))
                opbegin = c;
            else if (!strcmp(argv[c], "-d"))
                opbegin = c;
            else if (!strcmp(argv[c], "-x"))
                opbegin = c;
            else if (!strcmp(argv[c], "-l"))
                opbegin = c;
            else if (!strcmp(argv[c], "-a"))
                opbegin = c;
        }
        if (opbegin != -ENOENT)
            ret = submain(argc - opbegin, argv + opbegin);
    }

    // The following code must be executed after all instances of class rom are destroyed!
    if (MTR.fc != MTR.mc) {
        DERROR(
            "# MEMORY MANAGER TRAP\n"
            "#\tmalloc  counter: %d\n"
            "#\tfree    counter: %d\n"
            "#\trealloc counter: %d\n",
            MTR.mc,
            MTR.fc,
            MTR.rc);
    }
    return ret;
}

int submain(int argc, char** argv) {
    int ret = RET_OK;
    if (VERB())
        std::cout << "#PlayStation2 ROM Manager v" << MAJOR << "." << MINOR << "." << PATCH << " compiled " << buildate << "\n";
    rom ROMIMG;
    if (!strcmp(argv[0], "-l") && argc >= 2) {
        if (!ROMIMG.open(argv[1])) {
            ret = ROMIMG.displayContents();
        } else {
            DERROR("Could not find ROMFS filesystem on image\n");
        }
    } else if (!strcmp(argv[0], "-x") && argc >= 2) {
        if (!ROMIMG.open(argv[1])) {
            if (argc == 2) {
                ret = ROMIMG.dumpContents();
            } else {
                for (int i = 2; i < argc; i++) {
                    if ((ret = ROMIMG.dumpContents(argv[i])) != RET_OK)
                        break;
                }
            }
        } else {
            DERROR("Could not find ROMFS filesystem on image\n");
        }
    } else if (!strcmp(argv[0], "-g") && argc >= 4) {
        std::string confFilePath = argv[1];
        std::string folderPath = argv[2];
        std::string romFilePath = argv[3];
        if (util::fileExists(romFilePath)) {
            DERROR("\nOutput file already exists: %s \nAborting!\n", romFilePath.c_str());
            return -EEXIST;
        }
        ret = generateRomFromConf(confFilePath, romFilePath, folderPath);
    } else if (!strcmp(argv[0], "-a") && argc >= 2) {
        if (!ROMIMG.open(argv[1])) {
            for (int i = 2; i < argc; i++) {
                if ((ret = ROMIMG.addFile(argv[i], false, true, 0)) != RET_OK)
                    break;
            }
            if (ret == RET_OK)
                ret = ROMIMG.write(argv[1]);
        } else {
            DERROR("Could not find ROMFS filesystem on image\n");
        }
    }

err:
    if (ret != RET_OK) {
    }
    return 0;
}

enum SS {
    IMAGE_DECL = (1 << 0),  // image declared
    OFFLS_DECL = (1 << 1),  // offset table declared
};

#define np std::string::npos

off_t GetFileSize(std::string filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -ENOENT;
}

class fixfile {
   public:
    fixfile(std::string P1, size_t P2) : fname(P1), offset(P2), fsize(GetFileSize(P1)) { /*DCOUT("Add file "<< fname << " off:" << offset << " siz:" << fsize <<"\n");*/ };
    fixfile(std::string P1, size_t P2, size_t P3) : fname(P1), offset(P2), fsize(P3) { /*DCOUT("Add file "<< fname << " off:" << offset << " siz:" << fsize <<"\n");*/ };
    std::string fname;
    size_t offset;
    off_t fsize;
    static bool CompareOffset(fixfile A, fixfile B) { return (A.offset < B.offset); }
    static bool CompareSize(fixfile A, fixfile B) { return (A.fsize > B.fsize); }
};

#define HEX(x) std::hex << x << std::dec
#define CHKFILE(x) (GetFileSize(x) < 0) ? -ENOENT : RET_OK
#define CHKRESET(x)                       \
    if (util::Basename(x) == "RESET") {   \
        ROMIMG.addFile(x, true, true, 0); \
        continue;                         \
    }

std::vector<fixfile> FFiles;
std::vector<fixfile> CFiles;

#define FFMT "%-25s %-8ld %-8ld"
#define MAX_DEADGAP 128
int WriteImage(rom* ROM) {
    int ret = RET_OK;
    bool gapforce = false;
    off_t writtenbytes = 0x0;
    off_t Tdeadgap = 0x0;
    size_t rem = CFiles.size() + FFiles.size();

    std::sort(FFiles.begin(), FFiles.end(), fixfile::CompareOffset);
    int off = 0;
    for (size_t o = 0, off = 0; o < FFiles.size(); o++) {
        if (off >= FFiles[o].offset) {
            DERROR(
                "FATAL ERROR: Fixed file at position %lu collides with %lu\n"
                "File 1: " FFMT
                "\n"
                "File 2: " FFMT "\n",
                o - 1, o,
                FFiles[o - 1].fname.c_str(), FFiles[o - 1].fsize, FFiles[o - 1].offset,
                FFiles[o].fname.c_str(), FFiles[o].fsize, FFiles[o].offset);
            ret = -EIMPOSSIBLE;
        }
        off = FFiles[o].offset + FFiles[o].fsize;  // record where this fixed file ends so next iteration can detect a clash
    }

    printf("%-25s %-8s %-8s\n", "Files", "size", "offset");
    for (size_t x = 0, z = 0; x < CFiles.size() || z < FFiles.size();) {
        off_t next_off = (writtenbytes + CFiles[x].fsize);
        if (z < FFiles.size()) {                // if fix pos file list is not empty
            if (next_off > FFiles[z].offset) {  // next file clashes
                off_t gap = FFiles[z].offset - writtenbytes;
                if (gap < MAX_DEADGAP || !(x < CFiles.size()) || gapforce) {  // gap is smaller than 256 || ran out of common files
                    gapforce = false;
                    ROM->addDummy("-", gap);
                    printf(DGREY FFMT DEFCOL "\n", "-", gap, writtenbytes);
                    writtenbytes += gap;
                    Tdeadgap += gap;
                } else {
                    // DPRINTF("Gap too long. looking for another file\n");
                    size_t chosen = 0;
                    for (size_t a = x; a < CFiles.size(); a++) {
                        if (CFiles[a].fsize < gap) {  // file fits gap
                            if (chosen) {
                                if (CFiles[chosen].fsize < CFiles[a].fsize)
                                    chosen = a;
                            } else
                                chosen = a;
                        }
                    }
                    if (chosen) {  // found gap filler. write it down and remove it from the array to avoid parsing it twice
                        printf(FFMT "\n", CFiles[chosen].fname.c_str(), CFiles[chosen].fsize, writtenbytes);
                        ROM->addFile(CFiles[chosen].fname, false, true, 0);
                        writtenbytes += CFiles[chosen].fsize;
                        CFiles.erase(CFiles.begin() + chosen);
                    } else
                        gapforce = true;  // gap filler not found, force the usage of blank gap filler.
                    continue;
                }
                printf(YELBOLD FFMT " (%ld)" DEFCOL "\n", FFiles[z].fname.c_str(), FFiles[z].fsize, writtenbytes, FFiles[z].offset);
                if (writtenbytes != FFiles[z].offset) {
                    DERROR("FATAL ERROR\tCOULD NOT POSITION FIXED FILE AT REQUESTED OFFSET\n\tRequested Off:%ld (%lx)\n\tReal Off:%ld (%lx)\n", FFiles[z].offset, FFiles[z].offset, writtenbytes, writtenbytes);
                    ret = -1;
                    break;
                }
                ROM->addFile(FFiles[z].fname, true, true, 0);
                writtenbytes += FFiles[z].fsize;
                z++;
                continue;
            } else if (!(x < CFiles.size())) {  // we have remaining fixed files, no more normal files!!!
                off_t gap = FFiles[z].offset - writtenbytes;
                ROM->addDummy("-", gap);
                printf(DGREY FFMT DEFCOL "\n", "-", gap, writtenbytes);
                writtenbytes += gap;
                Tdeadgap += gap;

                printf(YELBOLD FFMT " (%ld)" DEFCOL "\n", FFiles[z].fname.c_str(), FFiles[z].fsize, writtenbytes, FFiles[z].offset);
                ROM->addFile(FFiles[z].fname, true, true, 0);
                writtenbytes += FFiles[z].fsize;
                z++;
            }
            // next file doesnt clash
        }
        if (x < CFiles.size()) {  // condition in case we eat CFiles list before writing all FFiles
            printf(FFMT "\n", CFiles[x].fname.c_str(), CFiles[x].fsize, writtenbytes);
            ROM->addFile(CFiles[x].fname, false, true, 0);
            writtenbytes += CFiles[x].fsize;
            x++;
        }
    }
    if (ret == RET_OK)
        ROM->write();
    printf(
        "# Size of contents: %ld\n"
        "# Files written: %lu (%lu fixed, %lu normal)\n"
        "# Space spent in dummy gaps: %ld\n"
        "",
        writtenbytes, rem, FFiles.size(), CFiles.size(),
        Tdeadgap);

    return RET_OK;
}

int help() {
    printf(
        "# Supported commands:\n"
        "\t<image> or -l <image>\n"
        "\t\tLists the contents of the image.\n"

        "\t-g <configuration file> <folder with files> <new_image>\n"
        "\t\tCreates a new ROM image with files from the specified folder based on the configuration file.\n"
        "\t\tYou can add files by adding lines to the configuration file in the following format:\n"
        "\t\t<module name>,<fixed offset>,<modification date>,<file version>,<comment>\n"
        "\t\tFor example: USBD,,,,\n"
        "\t\tAll fields except the module name can be empty. The modification date can be set to '-' to indicate that the date should not be written back.\n"

        "\t-x <image> [<files...>]\n"
        "\t\tExtracts the contents of the ROM image.\n"
        "\t\tIf no additional parameters are provided, it dumps the entire image to a subfolder named ext_<image> near the ROM.\n"
        "\t\tAdditionally, it also creates a CSV file with information about all extracted files. This file can be used later with the -g command.\n"
        "\t\tAlso, if some of the files inside the ROM are ROMs themselves, the app will also extract their contents into a subfolder.\n"

        "\t-a <image> <files...>\n"
        "\t\tAdds file(s) to an existing image. A fast way to add a file to an image without any advanced tweaks.\n");
    return 1;
}
